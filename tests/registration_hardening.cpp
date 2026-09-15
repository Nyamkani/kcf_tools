#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf/process/process_runtime.hpp"
#include "kcf/ipc/publisher.hpp"
#include "kcf/parameter/parameter.hpp"
#include "kcf/service/service_server.hpp"
#include "kcf/dynamic/dynamic_topic_reader.hpp"
#include "kcf/dynamic/dynamic_parameter_client.hpp"
#include "kcf/dynamic/dynamic_service_client.hpp"
#include "kcf/introspection/introspection_client.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
#include <arpa/inet.h>
#include <csignal>
#include <unistd.h>

struct Value {float value;double wide;float array[2];};
namespace kcf {
template<> struct TypeDescriptorTraits<Value>{
    static constexpr bool defined=true;
    static TypeDescriptor Get() noexcept {
        auto d=MakeTypeDescriptor<Value>("tool.hardening.Value.v1");d.field_count=3;
        d.fields[0]=MakeField<Value,float>("value",offsetof(Value,value));
        d.fields[1]=MakeField<Value,double>("wide",offsetof(Value,wide));
        d.fields[2]=MakeField<Value,float[2]>("array",offsetof(Value,array));return d;
    }
};
}
// Link wrapping affects this executable only. Recreate real endpoints exactly
// between public Open/Get and the backend's next registration check.
std::function<void()> after_topic_open,after_parameter_open,after_parameter_get,after_service_open;
unsigned endpoint_queries=0,service_queries=0,parameter_opens=0,service_opens=0;
void Run(std::function<void()>& hook){if(hook){auto f=std::move(hook);hook={};f();}}
extern "C" int RealTopicOpen(kcf::DynamicTopicReader*,const std::string&,const kcf::TypeDescriptor&)
    asm("__real__ZN3kcf18DynamicTopicReader4OpenERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERKNS_14TypeDescriptorE");
extern "C" int TopicOpen(kcf::DynamicTopicReader*,const std::string&,const kcf::TypeDescriptor&)
    asm("__wrap__ZN3kcf18DynamicTopicReader4OpenERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERKNS_14TypeDescriptorE");
int TopicOpen(kcf::DynamicTopicReader* p,const std::string& n,const kcf::TypeDescriptor& d){int r=RealTopicOpen(p,n,d);if(!r)Run(after_topic_open);return r;}
extern "C" int RealParameterOpen(kcf::DynamicParameterClient*,const std::string&,const kcf::TypeDescriptor&)
    asm("__real__ZN3kcf22DynamicParameterClient4OpenERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERKNS_14TypeDescriptorE");
extern "C" int ParameterOpen(kcf::DynamicParameterClient*,const std::string&,const kcf::TypeDescriptor&)
    asm("__wrap__ZN3kcf22DynamicParameterClient4OpenERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERKNS_14TypeDescriptorE");
int ParameterOpen(kcf::DynamicParameterClient* p,const std::string& n,const kcf::TypeDescriptor& d){++parameter_opens;int r=RealParameterOpen(p,n,d);if(!r)Run(after_parameter_open);return r;}
extern "C" int RealParameterGet(kcf::DynamicParameterClient*,kcf::DynamicPayload&)
    asm("__real__ZN3kcf22DynamicParameterClient3GetERNS_14DynamicPayloadE");
extern "C" int ParameterGet(kcf::DynamicParameterClient*,kcf::DynamicPayload&)
    asm("__wrap__ZN3kcf22DynamicParameterClient3GetERNS_14DynamicPayloadE");
int ParameterGet(kcf::DynamicParameterClient* p,kcf::DynamicPayload& v){int r=RealParameterGet(p,v);if(!r)Run(after_parameter_get);return r;}
extern "C" int RealServiceOpen(kcf::DynamicServiceClient*,std::uint16_t,std::uint16_t,const kcf::TypeDescriptor&,const kcf::TypeDescriptor&,std::uint32_t,std::uint32_t)
    asm("__real__ZN3kcf20DynamicServiceClient4OpenEttRKNS_14TypeDescriptorES3_jj");
extern "C" int ServiceOpen(kcf::DynamicServiceClient*,std::uint16_t,std::uint16_t,const kcf::TypeDescriptor&,const kcf::TypeDescriptor&,std::uint32_t,std::uint32_t)
    asm("__wrap__ZN3kcf20DynamicServiceClient4OpenEttRKNS_14TypeDescriptorES3_jj");
int ServiceOpen(kcf::DynamicServiceClient* p,std::uint16_t port,std::uint16_t id,const kcf::TypeDescriptor& req,const kcf::TypeDescriptor& res,std::uint32_t timeout,std::uint32_t retries){
    ++service_opens;int r=RealServiceOpen(p,port,id,req,res,timeout,retries);if(!r)Run(after_service_open);return r;
}
extern "C" int RealEndpoints(kcf::IntrospectionClient*,const kcf::RuntimeInfo&,std::vector<kcf::EndpointInfo>&)
    asm("__real__ZN3kcf19IntrospectionClient13ListEndpointsERKNS_11RuntimeInfoERSt6vectorINS_12EndpointInfoESaIS5_EE");
extern "C" int Endpoints(kcf::IntrospectionClient*,const kcf::RuntimeInfo&,std::vector<kcf::EndpointInfo>&)
    asm("__wrap__ZN3kcf19IntrospectionClient13ListEndpointsERKNS_11RuntimeInfoERSt6vectorINS_12EndpointInfoESaIS5_EE");
int Endpoints(kcf::IntrospectionClient* p,const kcf::RuntimeInfo& r,std::vector<kcf::EndpointInfo>& out){++endpoint_queries;return RealEndpoints(p,r,out);}
extern "C" int RealServices(kcf::IntrospectionClient*,const kcf::RuntimeInfo&,std::vector<kcf::ServiceInfo>&)
    asm("__real__ZN3kcf19IntrospectionClient12ListServicesERKNS_11RuntimeInfoERSt6vectorINS_11ServiceInfoESaIS5_EE");
extern "C" int Services(kcf::IntrospectionClient*,const kcf::RuntimeInfo&,std::vector<kcf::ServiceInfo>&)
    asm("__wrap__ZN3kcf19IntrospectionClient12ListServicesERKNS_11RuntimeInfoERSt6vectorINS_11ServiceInfoESaIS5_EE");
int Services(kcf::IntrospectionClient* p,const kcf::RuntimeInfo& r,std::vector<kcf::ServiceInfo>& out){++service_queries;return RealServices(p,r,out);}

struct Element:kcf::ProcessElement {
    std::string prefix="/kt71_"+std::to_string(getpid());
    kcf::Publisher<Value> publisher;kcf::Parameter<Value> owner;kcf::ServiceServer server;
    std::uint16_t port=0;std::atomic<unsigned> calls{0};std::atomic<bool> ready{false};
    void CreateService(){
        assert(server.Create(port)==0);
        assert((server.Register<Value,Value>(1,prefix+"/service",[&](const Value& request,Value& response){++calls;response=request;})==0));
        assert(server.Start()==0);
    }
    int Setup()override {
        int fd=socket(AF_INET,SOCK_DGRAM,0);assert(fd>=0);sockaddr_in address{};
        address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        assert(bind(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0);
        socklen_t size=sizeof(address);assert(getsockname(fd,reinterpret_cast<sockaddr*>(&address),&size)==0);
        port=ntohs(address.sin_port);close(fd);
        assert(publisher.Create(prefix+"/topic")==0);assert(publisher.Publish({1,2,{3,4}})==0);
        assert(owner.Create(prefix+"/parameter",{1,2,{3,4}})==0);CreateService();ready=true;return 0;
    }
    int Loop()override{return 0;}
    void RecreateTopic(){assert(publisher.Close()==0);assert(publisher.Unlink()==0);assert(publisher.Create(prefix+"/topic")==0);assert(publisher.Publish({9,2,{3,4}})==0);}
    void RecreateParameter(){assert(owner.Close()==0);assert(owner.Unlink()==0);assert(owner.Create(prefix+"/parameter",{9,2,{3,4}})==0);}
    void RecreateService(){server.Stop();CreateService();}
    void Shutdown()override{server.Stop();owner.Close();owner.Unlink();publisher.Close();publisher.Unlink();}
};
int main(){
    alarm(45);Element element;
    std::thread checks([&]{
        while(!element.ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        using namespace kcf_tool;KcfBackend backend;DataSnapshot output,candidate;
        EndpointIdentity topic,parameter,service;
        const auto refresh=[&]{assert(backend.Refresh()==0);topic=backend.GetTopics().at(0).identity;
            parameter=backend.GetParameters().at(0).identity;service=backend.QueryServices().at(0).identity;};
        refresh();assert(backend.GetParameter(parameter,candidate)==0);
        assert(backend.ReadTopicLatest(topic,output)==0 && output.fields[0].value=="1");
        const auto original=topic;element.RecreateTopic();
        output.type_name="sentinel";
        assert(backend.StartTopicEcho(topic)==-ESTALE && backend.ReadTopicLatest(topic,output)==-ESTALE);
        assert(output.type_name=="sentinel");refresh();assert(!(topic==original));
        assert(backend.StartTopicEcho(topic)==0);const auto queries=endpoint_queries;
        assert(backend.ReadTopicEcho(output)==0 && output.fields[0].value=="9" && endpoint_queries==queries);
        backend.StopTopicEcho();
        for(bool echo:{false,true}){after_topic_open=[&]{element.RecreateTopic();};
            assert((echo?backend.StartTopicEcho(topic):backend.ReadTopicLatest(topic,output))==-ESTALE);assert(!after_topic_open);refresh();}
        element.RecreateParameter();output.type_name="sentinel";
        assert(backend.GetParameter(parameter,output)==-ESTALE && output.type_name=="sentinel");
        assert(backend.SetParameter(parameter,candidate)==-ESTALE);Value typed{};assert(element.owner.Get(typed)==0 && typed.value==9);
        refresh();assert(backend.GetParameter(parameter,output)==0 && output.fields[0].value=="9");
        candidate.fields[0].value="5";assert(backend.SetParameter(parameter,candidate)==0);
        assert(element.owner.Get(typed)==0 && typed.value==5);
        after_parameter_open=[&]{element.RecreateParameter();};assert(backend.GetParameter(parameter,output)==-ESTALE);assert(!after_parameter_open);refresh();
        kcf::SharedParameter<Value> old;assert(old.Open(element.prefix+"/parameter")==0);
        after_parameter_get=[&]{element.RecreateParameter();};assert(backend.SetParameter(parameter,candidate)==-ESTALE);assert(!after_parameter_get);
        assert(old.Get(typed)==0 && typed.value==9);old.Close();assert(element.owner.Get(typed)==0 && typed.value==9);refresh();
        const auto old_service=backend.QueryServices().at(0);element.RecreateService();
        assert(backend.CallService(service,candidate,output)==-ESTALE && element.calls==0);
        refresh();const auto next_service=backend.QueryServices().at(0);
        assert(!(old_service.identity==next_service.identity) && old_service.port==next_service.port);
        assert(backend.CallService(service,candidate,output)==0 && element.calls==1);
        after_service_open=[&]{element.RecreateService();};assert(backend.CallService(service,candidate,output)==-ESTALE);
        assert(!after_service_open && element.calls==1);refresh();
        const auto endpoints_before=endpoint_queries,services_before=service_queries;
        assert(backend.ValidateParameter(parameter,candidate)==0 && backend.ValidateService(service,candidate)==0);
        assert(endpoint_queries==endpoints_before && service_queries==services_before);
        const auto p_opens=parameter_opens,s_opens=service_opens;
        for(const char* text:{"nan","-nan","inf","-inf","NaN","Infinity","1e9999"})for(int field=0;field<3;++field){
            auto invalid=candidate;if(field==2)invalid.fields[field].array_values[0]=text;else invalid.fields[field].value=text;
            assert(backend.ValidateParameter(parameter,invalid)==-ERANGE && backend.ValidateService(service,invalid)==-ERANGE);
            assert(backend.SetParameter(parameter,invalid)==-ERANGE && backend.CallService(service,invalid,output)==-ERANGE);
        }
        assert(parameter_opens==p_opens && service_opens==s_opens && element.calls==1);
        assert(element.owner.Get(typed)==0 && typed.value==9);
        assert(backend.SetParameter(parameter,candidate)==0 && backend.CallService(service,candidate,output)==0);
        assert(element.calls==2);
        // Registration-only replacement: the underlying Parameter SHM stays
        // alive. R4.1 alone cannot reject a stale client registration here.
        kcf::Parameter<Value> peer;assert(peer.Open(element.prefix+"/parameter")==0);
        const auto client_identity=[&]{assert(backend.Refresh()==0);
            for(const auto& p:backend.GetParameters())if(p.role=="CLIENT")return p.identity;
            assert(false);return EndpointIdentity{};};
        auto client_id=client_identity();assert(peer.Close()==0 && peer.Open(element.prefix+"/parameter")==0);
        assert(backend.GetParameter(client_id,output)==-ESTALE && backend.SetParameter(client_id,candidate)==-ESTALE);
        client_id=client_identity();candidate.fields[0].value="17";
        after_parameter_get=[&]{assert(peer.Close()==0 && peer.Open(element.prefix+"/parameter")==0);};
        assert(backend.SetParameter(client_id,candidate)==-ESTALE && !after_parameter_get);
        assert(element.owner.Get(typed)==0 && typed.value==5);peer.Close();
        assert(kill(getpid(),SIGTERM)==0);
    });
    kcf::ProcessRuntime runtime;runtime.SetLoopFrequency(100);const int result=runtime.Run(element);checks.join();assert(result==0);
    std::cout<<"PASS: registration replacement, Open/seed races, finite inputs, no discovery in validation or Echo ticks\n";
}
