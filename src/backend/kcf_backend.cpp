#include "kcf_tool/backend/kcf_backend.hpp"
#include "value_codec.hpp"
#include "kcf/introspection/introspection_client.hpp"
#include "kcf/dynamic/dynamic_topic_reader.hpp"
#include "kcf/dynamic/dynamic_parameter_client.hpp"
#include "kcf/dynamic/dynamic_service_client.hpp"
#include <algorithm>
#include <cerrno>
#include <new>
#include <utility>
#include <poll.h>
#include <sys/syscall.h>
#include <unistd.h>
namespace kcf_tool {
namespace {
template<class F> int Guard(F f){try{return f();}catch(const std::bad_alloc&){return -ENOMEM;}catch(...){return -EIO;}}
std::string State(kcf::ProcessState s){switch(s){case kcf::ProcessState::STOPPED:return "STOPPED";case kcf::ProcessState::STARTING:return "STARTING";case kcf::ProcessState::RUNNING:return "RUNNING";case kcf::ProcessState::STOPPING:return "STOPPING";case kcf::ProcessState::ERROR:return "ERROR";}return "UNKNOWN";}
std::string State(kcf::ApplicationState s){switch(s){case kcf::ApplicationState::INITIALIZING:return "INITIALIZING";case kcf::ApplicationState::RUNNING:return "RUNNING";case kcf::ApplicationState::ERROR:return "ERROR";case kcf::ApplicationState::RESETTING:return "RESETTING";case kcf::ApplicationState::SHUTTING_DOWN:return "SHUTTING_DOWN";}return "UNKNOWN";}
std::string Display(const DataSnapshot& s){if(s.fields.size()==1&&s.fields[0].array_values.empty())return s.fields[0].value;std::string text;
    for(const auto& f:s.fields){if(!text.empty())text+="; ";text+=f.name+"=";if(f.array_values.empty())text+=f.value;else{ text+="[";for(std::size_t i=0;i<f.array_values.size();++i){if(i)text+=",";text+=f.array_values[i];}text+="]";}}return text;}
template<class T> struct Record {T model;kcf::RuntimeInfo runtime;};
template<class T> Record<T>* Find(std::vector<Record<T>>& list,const EndpointIdentity& id){for(auto& r:list)if(r.model.identity==id)return &r;return nullptr;}
template<class T> int Select(const std::vector<Record<T>>& list,const std::string& name,const char* preferred,EndpointIdentity& id){
    const Record<T>* selected=nullptr;bool best=false;std::size_t matches=0;
    for(const auto& r:list)if(r.model.name==name){const bool priority=r.model.role==preferred;
        if(!selected||(priority&&!best)){selected=&r;best=priority;matches=1;}
        else if(priority==best)++matches;
    }
    if(!selected)return -ENOENT;
    if(matches!=1)return -ENOTUNIQ;
    id=selected->model.identity;return 0;
}
template<class T> std::vector<T> Models(const std::vector<Record<T>>& records){std::vector<T> result;for(const auto& r:records)result.push_back(r.model);return result;}
}
struct KcfBackend::Impl {
    struct PreparedTopic {
        EndpointIdentity identity;
        kcf::TypeDescriptor descriptor;
        kcf::DynamicTopicReader reader;
        int pidfd{-1};
        ~PreparedTopic(){if(pidfd>=0)::close(pidfd);}
    };
    std::unique_ptr<PreparedTopic> prepared;
    BackendConnectionState state{BackendConnectionState::DISCONNECTED};
    kcf::IntrospectionClient client;
    std::vector<ApplicationInfo> applications;std::vector<ElementInfo> elements;
    std::vector<Record<TopicInfo>> topics;std::vector<Record<ParameterInfo>> parameters;std::vector<Record<ServiceInfo>> services;
    static int DiscoveryError(int result){return result==-ENOENT || result==-ESRCH ? -ESTALE : result;}
    template<class T> int ValidateEndpoint(const Record<T>& record,kcf::EndpointKind kind,kcf::EndpointRole role){
        const auto& cached=record.model;
        if(cached.identity.runtime.pid!=record.runtime.pid ||
           cached.identity.runtime.process_start_ticks!=record.runtime.process_start_ticks)return -ESTALE;
        std::vector<kcf::EndpointInfo> current;
        const int result=client.ListEndpoints(record.runtime,current);
        if(result)return DiscoveryError(result);
        for(const auto& endpoint:current)if(endpoint.registration_id==cached.identity.registration_id){
            return endpoint.kind==kind && endpoint.role==role && cached.name==endpoint.name &&
                cached.type_id==endpoint.type_id && cached.payload_size==endpoint.payload_size ? 0 : -ESTALE;
        }
        return -ESTALE;
    }
    int ValidateRegistration(const Record<TopicInfo>& record){
        if(record.model.role!="PUBLISHER" && record.model.role!="SUBSCRIBER")return -ESTALE;
        return ValidateEndpoint(record,kcf::EndpointKind::TOPIC,record.model.role=="PUBLISHER"?
            kcf::EndpointRole::PUBLISHER:kcf::EndpointRole::SUBSCRIBER);
    }
    int ValidateRegistration(const Record<ParameterInfo>& record){
        if(record.model.role!="OWNER" && record.model.role!="CLIENT")return -ESTALE;
        return ValidateEndpoint(record,kcf::EndpointKind::PARAMETER,record.model.role=="OWNER"?
            kcf::EndpointRole::PARAMETER_OWNER:kcf::EndpointRole::PARAMETER_CLIENT);
    }
    int ValidateRegistration(const Record<ServiceInfo>& record){
        const auto& cached=record.model;
        if(cached.identity.runtime.pid!=record.runtime.pid ||
           cached.identity.runtime.process_start_ticks!=record.runtime.process_start_ticks)return -ESTALE;
        std::vector<kcf::ServiceInfo> current;
        const int result=client.ListServices(record.runtime,current);
        if(result)return DiscoveryError(result);
        for(const auto& service:current)if(service.registration_id==cached.identity.registration_id){
            return cached.name==service.name && cached.port==service.port && cached.service_id==service.service_id &&
                cached.request_type_id==service.request_type_id && cached.response_type_id==service.response_type_id &&
                cached.request_size==service.request_size && cached.response_size==service.response_size ? 0 : -ESTALE;
        }
        return -ESTALE;
    }
    int Type(const kcf::RuntimeInfo& runtime,std::uint64_t id,std::uint64_t size,kcf::TypeDescriptor& d){if(!id)return -ENOTSUP;int r=client.GetType(runtime,id,d);if(r)return r;return d.type_id==id&&d.payload_size==size&&kcf::ValidateTypeDescriptor(d)?0:-EPROTOTYPE;}
};
KcfBackend::KcfBackend():impl_(std::make_unique<Impl>()){}
KcfBackend::~KcfBackend()=default;
BackendConnectionState KcfBackend::GetConnectionState() const{return impl_->state;}
std::vector<ApplicationInfo> KcfBackend::GetApplications(){return impl_->applications;}
ApplicationInfo KcfBackend::GetApplicationInfo(){if(impl_->state!=BackendConnectionState::CONNECTED)return {"","DISCONNECTED"};if(impl_->applications.size()==1)return impl_->applications.front();if(impl_->applications.empty())return {"Standalone","DISCOVERED"};return {std::to_string(impl_->applications.size())+" applications","DISCOVERED"};}
std::vector<ElementInfo> KcfBackend::GetElements(){return impl_->elements;}
std::vector<TopicInfo> KcfBackend::GetTopics(){return Models(impl_->topics);}
std::vector<ParameterInfo> KcfBackend::GetParameters(){return Models(impl_->parameters);}
std::vector<ServiceInfo> KcfBackend::QueryServices(){return Models(impl_->services);}
int KcfBackend::Refresh(){CloseTopicRead();int result=Guard([&]{
    auto next=std::make_unique<Impl>();std::vector<kcf::SupervisorInfo> supervisors;std::vector<kcf::RuntimeInfo> runtimes;
    int r=next->client.ListSupervisors(supervisors);if(r)return r;r=next->client.ListRuntimes(runtimes);if(r)return r;
    for(const auto& s:supervisors){ApplicationInfo app;app.name=s.application_name;app.state=State(s.application_state);app.supervisor={s.pid,s.process_start_ticks};app.revision=s.revision;app.managed_element_count=s.element_count;next->applications.push_back(app);}
    for(const auto& runtime:runtimes){
        ElementInfo e{};e.identity={runtime.pid,runtime.process_start_ticks};e.pid=runtime.pid;e.executable=runtime.executable;e.name=e.executable;e.state=State(runtime.state);e.runtime_error=runtime.runtime_error;
        e.mode=runtime.execution_mode==kcf::ExecutionMode::STANDALONE?"STANDALONE":"SUPERVISED";e.application_name=e.mode=="STANDALONE"?"Standalone":"Unassigned";
        for(const auto& s:supervisors)for(std::uint32_t i=0;i<s.element_count;++i){const auto& member=s.elements[i];if(member.pid==runtime.pid&&member.process_start_ticks==runtime.process_start_ticks){e.name=member.name;e.supervisor={s.pid,s.process_start_ticks};e.application_name=s.application_name;}}
        next->elements.push_back(e);
        std::vector<kcf::EndpointInfo> endpoints;
        if(next->client.ListEndpoints(runtime,endpoints)==0)for(const auto& endpoint:endpoints){EndpointIdentity id{e.identity,endpoint.registration_id};
            if(endpoint.kind==kcf::EndpointKind::TOPIC){TopicInfo t{};t.identity=id;t.name=endpoint.name;t.type_name=endpoint.diagnostic_type_name;t.diagnostic_type_name=endpoint.diagnostic_type_name;t.type_id=endpoint.type_id;t.payload_size=endpoint.payload_size;
                t.role=endpoint.role==kcf::EndpointRole::PUBLISHER?"PUBLISHER":"SUBSCRIBER";if(t.role=="PUBLISHER")t.publishers.push_back(e.name);else t.subscribers.push_back(e.name);next->topics.push_back({t,runtime});}
            if(endpoint.kind==kcf::EndpointKind::PARAMETER){ParameterInfo p{};p.identity=id;p.name=endpoint.name;p.type_name=endpoint.diagnostic_type_name;p.diagnostic_type_name=endpoint.diagnostic_type_name;p.type_id=endpoint.type_id;p.payload_size=endpoint.payload_size;
                p.role=endpoint.role==kcf::EndpointRole::PARAMETER_OWNER?"OWNER":"CLIENT";p.text_editable=false;p.writable=false;
                kcf::TypeDescriptor d;if(next->Type(runtime,p.type_id,p.payload_size,d)==0){p.type_name=d.type_name;p.writable=true;p.text_editable=d.field_count==1&&d.fields[0].array_count==1;}
                next->parameters.push_back({p,runtime});}
        }
        std::vector<kcf::ServiceInfo> services;
        if(next->client.ListServices(runtime,services)==0)for(const auto& service:services){ServiceInfo s;s.identity={e.identity,service.registration_id};s.name=service.name;s.port=service.port;s.service_id=service.service_id;s.request_size=service.request_size;s.response_size=service.response_size;s.request_type_id=service.request_type_id;s.response_type_id=service.response_type_id;s.diagnostic_request_type_name=service.diagnostic_request_type_name;s.diagnostic_response_type_name=service.diagnostic_response_type_name;next->services.push_back({s,runtime});}
    }
    // Discovery is not atomic. Recheck public identities after per-runtime work,
    // dropping processes that exited during the scan without failing the refresh.
    std::vector<kcf::RuntimeInfo> live;std::vector<kcf::SupervisorInfo> current_supervisors;
    r=next->client.ListRuntimes(live);if(r)return r;
    r=next->client.ListSupervisors(current_supervisors);if(r)return r;
    const auto alive=[&](const RuntimeIdentity& id){return std::any_of(live.begin(),live.end(),[&](const auto& v){return v.pid==id.pid&&v.process_start_ticks==id.process_start_ticks;});};
    next->elements.erase(std::remove_if(next->elements.begin(),next->elements.end(),[&](const auto& e){return !alive(e.identity);}),next->elements.end());
    const auto prune=[&](auto& records){records.erase(std::remove_if(records.begin(),records.end(),[&](const auto& e){return !alive(e.model.identity.runtime);}),records.end());};
    prune(next->topics);prune(next->parameters);prune(next->services);
    next->applications.clear();
    for(const auto& s:current_supervisors){ApplicationInfo app;app.name=s.application_name;app.state=State(s.application_state);app.supervisor={s.pid,s.process_start_ticks};app.revision=s.revision;app.managed_element_count=s.element_count;next->applications.push_back(app);}
    for(auto& e:next->elements){e.name=e.executable;e.supervisor={};e.application_name=e.mode=="STANDALONE"?"Standalone":"Unassigned";
        for(const auto& s:current_supervisors)for(std::uint32_t i=0;i<s.element_count;++i){const auto& m=s.elements[i];if(m.pid==e.identity.pid&&m.process_start_ticks==e.identity.process_start_ticks){e.name=m.name;e.supervisor={s.pid,s.process_start_ticks};e.application_name=s.application_name;}}}
    for(auto& topic:next->topics)for(const auto& e:next->elements)if(e.identity==topic.model.identity.runtime){
        if(topic.model.role=="PUBLISHER")topic.model.publishers={e.name};else topic.model.subscribers={e.name};}
    next->state=(!next->applications.empty()||!next->elements.empty())?BackendConnectionState::CONNECTED:BackendConnectionState::DISCONNECTED;impl_=std::move(next);return 0;
});if(result){impl_->state=BackendConnectionState::ERROR;impl_->applications.clear();impl_->elements.clear();impl_->topics.clear();impl_->parameters.clear();impl_->services.clear();}return result;}
int KcfBackend::GetTopicInfo(const std::string& name,TopicInfo& output){return Guard([&]{EndpointIdentity id;int r=Select(impl_->topics,name,"PUBLISHER",id);if(r)return r;auto value=Find(impl_->topics,id)->model;output=std::move(value);return 0;});}
int KcfBackend::ReadTopicLatest(const std::string& name,DataSnapshot& output){return Guard([&]{EndpointIdentity id;int r=Select(impl_->topics,name,"PUBLISHER",id);return r?r:ReadTopicLatest(id,output);});}
int KcfBackend::StartTopicEcho(const EndpointIdentity& id){return Guard([&]{
    CloseTopicRead();
    auto* record=Find(impl_->topics,id);if(!record)return -ENOENT;
    if(record->model.role!="PUBLISHER")return -ENOTSUP;
    int r=impl_->ValidateRegistration(*record);if(r)return r;
    auto prepared=std::make_unique<Impl::PreparedTopic>();
    // Open a process lifetime handle before public GetType validates start ticks.
    // poll(pidfd) never waits and cannot mistake a reused PID for this runtime.
    prepared->pidfd=static_cast<int>(::syscall(SYS_pidfd_open,id.runtime.pid,0));
    if(prepared->pidfd<0)return -errno;
    r=impl_->Type(record->runtime,record->model.type_id,record->model.payload_size,prepared->descriptor);
    if(r)return r;
    r=prepared->reader.Open(record->model.name,prepared->descriptor);if(r)return r;
    r=impl_->ValidateRegistration(*record);if(r)return r;
    prepared->identity=id;impl_->prepared=std::move(prepared);return 0;
});}
void KcfBackend::StopTopicEcho(){impl_->prepared.reset();}
int KcfBackend::PrepareTopicRead(const EndpointIdentity& id){return StartTopicEcho(id);}
void KcfBackend::CloseTopicRead(){StopTopicEcho();}
int KcfBackend::ReadTopicEcho(DataSnapshot& output){
    const int result=Guard([&]{
        if(!impl_->prepared)return -EBADF;
        auto& session=*impl_->prepared;
        pollfd lifetime{session.pidfd,POLLIN,0};
        const int ready=::poll(&lifetime,1,0);
        if(ready<0)return -errno;
        if(ready>0)return -ESTALE;
        kcf::DynamicPayload bytes;
        const int r=session.reader.ReadLatest(bytes);
        return r?r:detail::Decode(session.descriptor,bytes,output);
    });
    if(result && result!=-EAGAIN)StopTopicEcho();
    return result;
}
int KcfBackend::ReadTopicLatest(const EndpointIdentity& id,DataSnapshot& output){return Guard([&]{
    auto* record=Find(impl_->topics,id);if(!record)return -ENOENT;
    int r=impl_->ValidateRegistration(*record);if(r)return r;
    kcf::TypeDescriptor descriptor;
    r=impl_->Type(record->runtime,record->model.type_id,record->model.payload_size,descriptor);if(r)return r;
    kcf::DynamicTopicReader reader;r=reader.Open(record->model.name,descriptor);if(r)return r;
    r=impl_->ValidateRegistration(*record);if(r)return r;
    kcf::DynamicPayload bytes;r=reader.ReadLatest(bytes);return r?r:detail::Decode(descriptor,bytes,output);
});}
int KcfBackend::GetParameter(const EndpointIdentity& id,DataSnapshot& output){return Guard([&]{auto* record=Find(impl_->parameters,id);if(!record)return -ENOENT;
    int r=impl_->ValidateRegistration(*record);if(r)return r;
    kcf::TypeDescriptor d;r=impl_->Type(record->runtime,record->model.type_id,record->model.payload_size,d);if(r)return r;
    kcf::DynamicParameterClient client;r=client.Open(record->model.name,d);if(r)return r;
    r=impl_->ValidateRegistration(*record);if(r)return r;
    kcf::DynamicPayload bytes;r=client.Get(bytes);return r?r:detail::Decode(d,bytes,output);});}
int KcfBackend::GetParameter(const std::string& name,ParameterInfo& output){return Guard([&]{EndpointIdentity id;int r=Select(impl_->parameters,name,"OWNER",id);if(r)return r;auto* record=Find(impl_->parameters,id);auto model=record->model;
    r=GetParameter(id,model.snapshot);if(r)return r;model.type_name=model.snapshot.type_name;model.value=Display(model.snapshot);record->model=model;output=std::move(model);return 0;});}
int KcfBackend::ValidateParameter(const EndpointIdentity& id,const DataSnapshot& value){return Guard([&]{
    auto* record=Find(impl_->parameters,id);if(!record)return -ENOENT;
    if(!record->model.type_id)return -ENOTSUP;
    if(!record->model.writable)return -EPERM;
    kcf::TypeDescriptor d;int r=impl_->Type(record->runtime,record->model.type_id,record->model.payload_size,d);if(r)return r;
    kcf::DynamicPayload bytes;return detail::Encode(d,value,bytes);
});}
int KcfBackend::SetParameter(const EndpointIdentity& id,const DataSnapshot& value){return Guard([&]{auto* record=Find(impl_->parameters,id);if(!record)return -ENOENT;if(!record->model.type_id)return -ENOTSUP;if(!record->model.writable)return -EPERM;
    kcf::TypeDescriptor d;int r=impl_->Type(record->runtime,record->model.type_id,record->model.payload_size,d);if(r)return r;
    // Validate the full input before opening storage. Preserve non-field bytes.
    kcf::DynamicPayload bytes;r=detail::Encode(d,value,bytes);if(r)return r;
    r=impl_->ValidateRegistration(*record);if(r)return r;
    kcf::DynamicParameterClient client;r=client.Open(record->model.name,d);if(r)return r;
    kcf::DynamicPayload seed;r=client.Get(seed);if(r)return r;r=detail::Encode(d,value,bytes,&seed);if(r)return r;
    r=impl_->ValidateRegistration(*record);return r?r:client.Set(bytes);});}
int KcfBackend::SetParameter(const std::string& name,const std::string& text){return Guard([&]{EndpointIdentity id;int r=Select(impl_->parameters,name,"OWNER",id);if(r)return r;const auto* record=Find(impl_->parameters,id);if(!record->model.type_id)return -ENOTSUP;if(!record->model.writable)return -EPERM;if(!record->model.text_editable)return -ENOTSUP;
    DataSnapshot value;r=GetParameter(id,value);if(r)return r;value.fields[0].value=text;return SetParameter(id,value);});}
int KcfBackend::ValidateService(const EndpointIdentity& id,const DataSnapshot& request){return Guard([&]{
    auto* record=Find(impl_->services,id);if(!record)return -ENOENT;
    const auto& s=record->model;kcf::TypeDescriptor req,res;
    int r=impl_->Type(record->runtime,s.request_type_id,s.request_size,req);if(r)return r;
    r=impl_->Type(record->runtime,s.response_type_id,s.response_size,res);if(r)return r;
    kcf::DynamicPayload bytes;return detail::Encode(req,request,bytes);
});}
int KcfBackend::CallService(const EndpointIdentity& id,const DataSnapshot& request,DataSnapshot& output,std::uint32_t timeout,std::uint32_t retries){return Guard([&]{auto* record=Find(impl_->services,id);if(!record)return -ENOENT;const auto& s=record->model;kcf::TypeDescriptor req,res;int r=impl_->Type(record->runtime,s.request_type_id,s.request_size,req);if(r)return r;r=impl_->Type(record->runtime,s.response_type_id,s.response_size,res);if(r)return r;
    kcf::DynamicPayload bytes,response;r=detail::Encode(req,request,bytes);if(r)return r;
    r=impl_->ValidateRegistration(*record);if(r)return r;
    kcf::DynamicServiceClient client;r=client.Open(s.port,s.service_id,req,res,timeout,retries);if(r)return r;
    r=impl_->ValidateRegistration(*record);if(r)return r;
    r=client.Call(bytes,response);return r?r:detail::Decode(res,response,output);});}
int KcfBackend::GetTypeTemplate(const RuntimeIdentity& identity,std::uint64_t type_id,DataSnapshot& output){return Guard([&]{
    if(!type_id)return -ENOTSUP;
    if(std::none_of(impl_->elements.begin(),impl_->elements.end(),[&](const auto& e){return e.identity==identity;}))return -ENOENT;
    kcf::RuntimeInfo runtime{};runtime.pid=identity.pid;runtime.process_start_ticks=identity.process_start_ticks;
    kcf::TypeDescriptor d;int r=impl_->client.GetType(runtime,type_id,d);if(r)return r;
    kcf::DynamicPayload bytes;bytes.type_id=d.type_id;bytes.bytes.resize(d.payload_size);return detail::Decode(d,bytes,output);
});}
int KcfBackend::StartTopicMonitor(const std::string&,TopicDataCallback){return -ENOTSUP;}
int KcfBackend::StopTopicMonitor(const std::string&){return -ENOTSUP;}
}
