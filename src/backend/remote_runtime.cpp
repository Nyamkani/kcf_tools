#include "remote_runtime.hpp"
#include <cerrno>
#include <cstring>
#include <atomic>
namespace kcf_tool {
namespace n=kcf::network;
namespace {
std::atomic<std::uint64_t> next_handle{0};
n::HostIdentity Host(const std::string& text){n::HostIdentity h;if(text.size()!=32)return h;for(unsigned i=0;i<16;++i){auto digit=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};int a=digit(text[2*i]),b=digit(text[2*i+1]);if(a<0||b<0)return {};h.bytes[i]=(a<<4)|b;}return h;}
n::RemoteEndpointIdentity Identity(const RemoteEndpoint& e){return {{Host(e.identity.host_id),Host(e.identity.boot_id),e.identity.pid,e.identity.start_ticks},e.identity.registration_id};}
n::TypeIdentity Type(const RemoteTypeIdentity& t){return {t.type_id,t.schema_id,t.encoding_id,t.payload_size};}
int Decode(const kcf::TypeDescriptor& d,const std::vector<std::uint8_t>& bytes,DataSnapshot& out){kcf::DynamicPayload p;int r=n::DecodeParameterValue(d,bytes,p);return r?r:detail::Decode(d,p,out);}
int Encode(const kcf::TypeDescriptor& d,const DataSnapshot& input,std::vector<std::uint8_t>& out){kcf::DynamicPayload p;int r=detail::Encode(d,input,p);return r?r:n::EncodeParameterValue(d,p,out);}
}
RemoteRuntime::RemoteRuntime(n::PeerDiscovery& d,const n::DiscoveryConfig& c):discovery_(d),control_(d,c),topics_(d,c,control_){error_=control_.Start();if(!error_)error_=topics_.Start();}
RemoteRuntime::~RemoteRuntime(){topics_.Stop();control_.Stop();}
void RemoteRuntime::CancelIO(){control_.Stop();}
bool RemoteRuntime::Current(const RemoteEndpoint& e) const {
    for(const auto& p:discovery_.GetPeers())if(p.host==Host(e.identity.host_id)){
        if(p.state!=n::PeerState::ONLINE||p.boot_id!=Host(e.identity.boot_id)||p.session_id!=e.session||!p.metadata_ready||!p.compatibility.Compatible())return false;
        for(const auto& m:p.endpoints)if(m.identity==Identity(e)&&m.name==e.name)return true;
    }return false;
}
int RemoteRuntime::Descriptor(const RemoteEndpoint& e,n::DescriptorRole role,n::TypeIdentity type,kcf::TypeDescriptor& d,std::uint32_t timeout){
    int r=control_.QueryTypeDescriptor(Identity(e),e.session,e.name,role,type,d,std::chrono::milliseconds(timeout));
    n::TypeIdentity actual;if(!r)r=n::ParameterWireType(d,actual);if(!r&&!(actual==type))r=-EPROTOTYPE;return r;
}
RemoteReply RemoteRuntime::Execute(const RemoteCommand& command){
    RemoteReply out;if(error_){out.error=error_;return out;}
    const auto& e=command.endpoint;auto timeout=std::chrono::milliseconds(command.timeout_ms);
    if(command.operation==RemoteOperation::Poll){
        for(auto& entry:actions_){auto& a=entry.second;auto s=a.handle->Snapshot();RemoteActivity v;v.handle=entry.first;v.endpoint=a.endpoint;
            v.accepted=s.accepted;v.terminal=s.terminal;v.state=static_cast<int>(s.state);v.error=s.error;v.communication_lost=s.communication_lost||!Current(a.endpoint);
            v.execution_unknown=s.execution_unknown;v.cancel_unknown=s.cancel_unknown;v.cancel_accepted=s.cancel_accepted;v.sequence=s.feedback_sequence;
            if(!v.communication_lost){if(!s.feedback.empty()){int code=Decode(a.feedback,s.feedback,v.feedback);v.has_value=!code;if(code)v.error=code;}if(!s.result.empty()){int code=Decode(a.result,s.result,v.result);if(code)v.error=code;}}
            out.activities.push_back(std::move(v));
        }
        for(auto& entry:subscriptions_){auto& t=entry.second;auto s=t.handle->Snapshot();RemoteActivity v;v.topic=true;v.handle=entry.first;v.endpoint=t.endpoint;
            v.state=static_cast<int>(s.state);v.error=s.error;v.sequence=s.sequence;v.received_packets=s.route.packets_received;v.route_id=s.route.route_id;v.observed_at=std::chrono::steady_clock::now();v.communication_lost=!Current(t.endpoint);
            v.terminal=s.state==n::RemoteTopicState::CLOSED||s.state==n::RemoteTopicState::ERROR||s.state==n::RemoteTopicState::LOST||s.state==n::RemoteTopicState::STALE;
            if(s.has_value&&!v.communication_lost&&!v.terminal){v.error=Decode(s.descriptor,s.payload,v.feedback);v.has_value=!v.error;v.feedback.sequence=s.sequence;}
            out.activities.push_back(std::move(v));
        }return out;
    }
    if(command.operation==RemoteOperation::Unsubscribe){auto it=subscriptions_.find(command.handle);if(it==subscriptions_.end())out.error=-ENOENT;else{out.error=topics_.Unsubscribe(it->second.handle);subscriptions_.erase(it);}return out;}
    if(!Current(e)){out.error=-ESTALE;return out;}
    if(command.operation==RemoteOperation::Cancel){auto it=actions_.find(command.handle);if(it==actions_.end()||!(Identity(it->second.endpoint)==Identity(e))||it->second.endpoint.session!=e.session){out.error=-ESTALE;return out;}
        auto r=control_.CancelAction(it->second.handle,timeout);out.error=r.error;out.response_received=r.response_received;out.cancel_unknown=r.cancel_unknown;out.accepted=r.accepted;return out;}
    if(command.operation==RemoteOperation::Subscribe){std::shared_ptr<n::RemoteTopicSubscription> subscription;out.error=topics_.Subscribe(Identity(e),e.session,e.name,Type(e.value_type),subscription,timeout);
        if(!out.error){out.handle=++next_handle;subscriptions_.emplace(out.handle,Topic{e,subscription});}return out;}
    const bool parameter=e.kind=="P";kcf::TypeDescriptor input;
    out.error=Descriptor(e,parameter?n::DescriptorRole::VALUE:n::DescriptorRole::REQUEST,Type(parameter?e.value_type:e.request_type),input,command.timeout_ms);
    if(out.error)return out;
    if(command.operation==RemoteOperation::Prepare){kcf::DynamicPayload zero;zero.type_id=input.type_id;zero.bytes.resize(input.payload_size);out.error=detail::Decode(input,zero,out.value);return out;}
    if(command.operation==RemoteOperation::Get){std::vector<std::uint8_t> bytes;out.error=control_.GetParameter(Identity(e),e.session,e.name,Type(e.value_type),bytes,timeout);out.response_received=!out.error;if(!out.error)out.error=Decode(input,bytes,out.value);return out;}
    std::vector<std::uint8_t> bytes;out.error=Encode(input,command.input,bytes);if(out.error)return out;
    if(command.operation==RemoteOperation::Set){auto r=control_.SetParameter(Identity(e),e.session,e.name,Type(e.value_type),bytes,timeout);
        out.error=r.error;out.execution_unknown=r.execution_unknown;out.response_received=r.response_received;
        if(!out.error){std::vector<std::uint8_t> readback;out.readback_error=control_.GetParameter(Identity(e),e.session,e.name,Type(e.value_type),readback,timeout);if(!out.readback_error)out.readback_error=Decode(input,readback,out.value);}return out;}
    kcf::TypeDescriptor response;out.error=Descriptor(e,n::DescriptorRole::RESPONSE,Type(e.response_type),response,command.timeout_ms);if(out.error)return out;
    if(command.operation==RemoteOperation::Call){auto r=control_.CallService(Identity(e),e.session,e.name,Type(e.request_type),Type(e.response_type),bytes,timeout);
        out.error=r.error;out.execution_unknown=r.execution_unknown;out.response_received=r.response_received;if(!out.error)out.error=Decode(response,r.value,out.value);return out;}
    if(command.operation==RemoteOperation::Goal){
        kcf::TypeDescriptor feedback;out.error=Descriptor(e,n::DescriptorRole::FEEDBACK,Type(e.feedback_type),feedback,command.timeout_ms);if(out.error)return out;
        // Retain active handles only; the UI retains completed activity history.
        for(auto it=actions_.begin();it!=actions_.end();)if(it->second.handle->Snapshot().terminal)it=actions_.erase(it);else ++it;
        auto r=control_.StartAction(Identity(e),e.session,e.name,Type(e.request_type),Type(e.feedback_type),Type(e.response_type),bytes,timeout,std::chrono::milliseconds(command.tracking_ms));
        out.error=r.error;out.response_received=r.response_received;out.execution_unknown=r.execution_unknown;out.accepted=r.response_received&&!r.error;
        if(r.handle){out.handle=++next_handle;actions_.emplace(out.handle,Action{e,feedback,response,r.handle});}return out;
    }
    out.error=-EINVAL;return out;
}
}
