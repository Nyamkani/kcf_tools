#include "kcf_tool/backend/kcf_backend.hpp"
#include <cerrno>
#ifdef KCF_TOOL_HAS_REMOTE_CONTROL
#include "remote_runtime.hpp"
#endif
#ifdef KCF_TOOL_HAS_NETWORK
#include "kcf/network/peer_discovery.hpp"
#ifdef KCF_TOOL_HAS_NETWORK_DIAGNOSTICS
#include "kcf/network/network_diagnostics_client.hpp"
#include "network_rates.hpp"
#endif
#include <sys/random.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <map>
#include <sstream>
#include <thread>

namespace kcf_tool {
namespace {
namespace net = kcf::network;
std::string Key(const net::HostIdentity& id) {
    constexpr char hex[]="0123456789abcdef";
    std::string out;
    for(auto b:id.bytes){out+=hex[b>>4];out+=hex[b&15];}
    return out;
}
std::string Key(const net::RemoteRuntimeIdentity& id) {
    return Key(id.host)+":"+Key(id.boot_id)+":"+std::to_string(id.pid)+":"+std::to_string(id.process_start_ticks);
}
std::string Key(const net::RemoteEndpointIdentity& id) {
    return Key(id.runtime)+":"+std::to_string(id.registration_id);
}
std::string Type(const net::TypeIdentity& t) {
    return "type="+std::to_string(t.type_id)+" schema="+std::to_string(t.schema_id)+
        " encoding="+std::to_string(t.encoding_id)+" bytes="+std::to_string(t.payload_size);
}
std::string Capabilities(net::CapabilitySet bits) {
    const char* names[]={"DISCOVERY","REMOTE_PARAMETER","REMOTE_SERVICE","REMOTE_ACTION","REMOTE_TOPIC",
        "TARGET_ENDPOINT","TARGET_HOST","TARGET_APPLICATION","TARGET_GROUP","TARGET_ALL","NETWORK_DIAGNOSTICS","TYPE_DESCRIPTOR_QUERY","REMOTE_TOPIC_RECEIVER","TOPIC_ENDPOINT_SUBSCRIPTION"};
    std::string out;
    for(unsigned i=0;i<14;++i)if(bits & (1ull<<i)){if(!out.empty())out+=", ";out+=names[i];}
    return out+" (raw="+std::to_string(bits)+")";
}
NetworkSnapshot Convert(const std::vector<net::PeerInfo>& peers, int error) {
    NetworkSnapshot out;out.status=error?"Discovery error: "+std::to_string(error):"Network discovery running (manual Refresh)";
    if(error)return out; // Do not present cached peers as live after worker failure.
    using Candidate=std::pair<const net::PeerInfo*,const net::NetworkEndpointMetadata*>;
    std::map<std::pair<net::EndpointKind,std::string>,std::vector<Candidate>> index;
    for(const auto& peer:peers)if(peer.metadata_ready)
        for(const auto& e:peer.endpoints)index[{e.kind,e.name}].push_back({&peer,&e});
    for(const auto& peer:peers){
        RemoteHost host;host.id=Key(peer.host);host.boot_id=Key(peer.boot_id);host.label=host.id;
        host.address=peer.address+":"+std::to_string(peer.port);
        host.state=peer.state==net::PeerState::ONLINE?"ONLINE":"LOST";
        host.session=peer.session_id;host.revision=peer.metadata_revision;host.metadata_ready=peer.metadata_ready;
        host.age_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-peer.last_seen).count();
        const auto& f=peer.profile.framework;const auto& n=peer.profile.network;
        host.framework_version=std::to_string(f.major)+"."+std::to_string(f.minor)+"."+std::to_string(f.patch);
        host.network_version=std::to_string(n.major)+"."+std::to_string(n.minor);
        host.compatibility=net::CompatibilityReasonName(peer.compatibility.reason);
        host.capabilities=Capabilities(peer.profile.capabilities);
        if(peer.metadata_ready)for(const auto& e:peer.endpoints){
            RemoteEndpoint endpoint;
            endpoint.identity={Key(e.identity.runtime.host),Key(e.identity.runtime.boot_id),e.identity.runtime.pid,e.identity.runtime.process_start_ticks,e.identity.registration_id};endpoint.session=peer.session_id;
            const auto wire=[](const net::TypeIdentity& t){return RemoteTypeIdentity{t.type_id,t.schema_id,t.encoding_id,t.payload_size};};
            endpoint.value_type=wire(e.value_type);endpoint.request_type=wire(e.request_type);endpoint.response_type=wire(e.response_type);endpoint.feedback_type=wire(e.feedback_type);endpoint.id=Key(e.identity);endpoint.runtime_id=Key(e.identity.runtime);
            endpoint.application_id=e.application.pid?Key(e.application):host.id+":"+host.boot_id+":standalone";
            endpoint.application=e.application_name.empty()?"Standalone":e.application_name;
            endpoint.element=e.element_name;endpoint.name=e.name;
            if(!e.host_name.empty())host.label=e.host_name;
            endpoint.kind=e.kind==net::EndpointKind::TOPIC?"T":e.kind==net::EndpointKind::PARAMETER?"P":e.kind==net::EndpointKind::SERVICE?"S":"A";
            const bool provider=e.role==net::EndpointRole::PROVIDER;
            endpoint.role=endpoint.kind=="T"?(provider?"PUBLISHER":"SUBSCRIBER"):
                endpoint.kind=="P"?(provider?"OWNER":"CLIENT"):(provider?"SERVER":"CONSUMER");
            endpoint.scope=e.scope==net::NetworkScope::REMOTE?"REMOTE":"LOCAL_ONLY";
            endpoint.type=(endpoint.kind=="T" || endpoint.kind=="P")?Type(e.value_type):
                "request/goal: "+Type(e.request_type)+"\nresponse/result: "+Type(e.response_type)+"\nfeedback: "+Type(e.feedback_type);
            // Pairwise diagnostics are observations, never proof of a route.
            std::size_t examined=0;
            for(const auto& pair:index.at({e.kind,e.name})){
                if(++examined>32){endpoint.diagnostic+="Pair comparison limit reached (32 candidates)\n";break;}
                const auto& other=*pair.first;const auto& candidate=*pair.second;
                if(other.host==peer.host || candidate.role==e.role)continue;
                const auto check=net::CheckEndpointCompatibility(peer.profile,other.profile,e,candidate);
                if(!check.Compatible())endpoint.diagnostic+=Key(candidate.identity)+": "+net::CompatibilityReasonName(check.reason)+"\n";
            }
            host.endpoints.push_back(std::move(endpoint));
        }
        std::sort(host.endpoints.begin(),host.endpoints.end(),[](const auto& a,const auto& b){return a.id<b.id;});
        out.hosts.push_back(std::move(host));
    }
    std::sort(out.hosts.begin(),out.hosts.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    return out;
}
#ifdef KCF_TOOL_HAS_NETWORK_DIAGNOSTICS
using Clock=std::chrono::steady_clock;
struct CachedDiagnostics {
    RemoteNetworkDiagnostics value;
    Clock::time_point next{};
    detail::NetworkRateTracker rates;
};
RouteEndpointIdentity ConvertIdentity(const net::RemoteEndpointIdentity& e) {
    return {Key(e.runtime.host),Key(e.runtime.boot_id),e.runtime.pid,e.runtime.process_start_ticks,e.registration_id};
}
RemoteNetworkDiagnostics ConvertDiagnostics(const net::NetworkDiagnosticsSnapshot& snapshot) {
    RemoteNetworkDiagnostics out;out.status=snapshot.truncated?DiagnosticsState::TRUNCATED:DiagnosticsState::VALID;
    out.gateway_host=Key(snapshot.gateway_host);out.gateway_boot=Key(snapshot.gateway_boot_id);out.gateway_session=snapshot.gateway_session;
    out.observed_at_unix_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    out.total_routes=snapshot.total_routes;out.truncated=snapshot.truncated;
    for(const auto& r:snapshot.routes){
        RemoteRoute v;v.source=ConvertIdentity(r.source);v.destination=ConvertIdentity(r.destination);
        v.source_session=r.source_session;v.destination_session=r.destination_session;v.route_id=r.route_id;
        v.name=r.name;v.state=static_cast<std::uint8_t>(r.state);v.reason=static_cast<std::uint8_t>(r.reason);
        v.proxy_recovery=static_cast<std::uint8_t>(r.proxy_recovery);v.sending=r.sending;v.error=r.error;
        v.packets_sent=r.packets_sent;v.packets_received=r.packets_received;v.samples_published=r.samples_published;
        v.dropped=r.dropped;v.duplicate=r.duplicate;v.out_of_order=r.out_of_order;v.missed_sequence=r.missed_sequence;
        v.malformed=r.malformed;v.local_samples_skipped=r.local_samples_skipped;out.routes.push_back(std::move(v));
    }
    return out;
}
DiagnosticsState Eligibility(const net::PeerInfo& peer,const net::DiscoveryConfig& config) {
    if(peer.state!=net::PeerState::ONLINE)return DiagnosticsState::LOST;
    if(!net::CheckPeerCompatibility(config.profile,peer.profile,config.compatibility_policy).Compatible())return DiagnosticsState::INCOMPATIBLE;
    if(!(peer.profile.capabilities&net::CapabilityBit(net::Capability::NETWORK_DIAGNOSTICS)))return DiagnosticsState::UNSUPPORTED;
    return DiagnosticsState::PENDING;
}
void Reconcile(std::map<std::string,CachedDiagnostics>& cache,const std::vector<net::PeerInfo>& peers,const net::DiscoveryConfig& config) {
    for(auto it=cache.begin();it!=cache.end();) {
        if(std::none_of(peers.begin(),peers.end(),[&](const auto& p){return Key(p.host)==it->first;}))it=cache.erase(it);else ++it;
    }
    for(const auto& peer:peers){auto& entry=cache[Key(peer.host)];auto& value=entry.value;const auto state=Eligibility(peer,config);
        if(value.gateway_boot!=Key(peer.boot_id)||value.gateway_session!=peer.session_id||state!=DiagnosticsState::PENDING||
           value.status==DiagnosticsState::LOST||value.status==DiagnosticsState::INCOMPATIBLE||value.status==DiagnosticsState::UNSUPPORTED){
            entry={};value.gateway_host=Key(peer.host);value.gateway_boot=Key(peer.boot_id);value.gateway_session=peer.session_id;value.status=state;
        }
    }
}
#endif

}
class NetworkObserver {
public:
    explicit NetworkObserver(NetworkOptions options):snapshot_([]{NetworkSnapshot s;s.status="Network starting";return s;}()),worker_([this,options]{Run(options);}) {}
    ~NetworkObserver(){ {std::lock_guard<std::mutex> lock(mutex_);stop_=true;} wake_.notify_all();
#ifdef KCF_TOOL_HAS_REMOTE_CONTROL
        std::shared_ptr<RemoteExecutor> executor;{std::lock_guard<std::mutex> lock(mutex_);executor=remote_.lock();}if(executor)executor->Stop();executor.reset();
#endif
        worker_.join();
    }
    std::future<RemoteReply> Submit(const RemoteCommand& c){
#ifdef KCF_TOOL_HAS_REMOTE_CONTROL
        std::lock_guard<std::mutex> lock(mutex_);auto executor=remote_.lock();return executor?executor->Submit(c):RemoteUnavailable(-ENOTCONN);
#else
        (void)c;return RemoteUnavailable();
#endif
    }
    NetworkSnapshot Snapshot(){std::lock_guard<std::mutex> lock(mutex_);return snapshot_;}
private:
    void Run(const NetworkOptions& options) noexcept {
        try {
            net::PeerDiscovery discovery;net::DiscoveryConfig config;
            // An observation instance has its own ephemeral identity; it never
            // impersonates a Gateway or advertises control/forwarding capability.
            if(getrandom(config.host.bytes.data(),16,0)!=16 || getrandom(config.boot_id.bytes.data(),16,0)!=16){
                std::lock_guard<std::mutex> lock(mutex_);snapshot_.status="Discovery identity generation failed";return;
            }
            if(!options.host_id.empty()){
                if(options.host_id.size()!=32){std::lock_guard<std::mutex> lock(mutex_);snapshot_.status="Invalid Host ID: expected 32 hexadecimal digits";return;}
                bool nonzero=false;
                for(unsigned i=0;i<16;++i){auto digit=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};int a=digit(options.host_id[2*i]),b=digit(options.host_id[2*i+1]);if(a<0||b<0){std::lock_guard<std::mutex> lock(mutex_);snapshot_.status="Invalid Host ID";return;}config.host.bytes[i]=(a<<4)|b;nonzero|=config.host.bytes[i]!=0;}
                if(!nonzero){std::lock_guard<std::mutex> lock(mutex_);snapshot_.status="Host ID must not be zero";return;}
#ifdef KCF_TOOL_HAS_REMOTE_CONTROL
                config.profile.capabilities=net::CapabilityBit(net::Capability::DISCOVERY)|net::CapabilityBit(net::Capability::REMOTE_TOPIC_RECEIVER)|net::CapabilityBit(net::Capability::TARGET_ENDPOINT);
#endif
            }
            config.interface_address=options.interface_address;config.port=options.port;
            const int result=discovery.Start(config);
            if(result){std::lock_guard<std::mutex> lock(mutex_);snapshot_.status="Discovery start failed: "+std::to_string(result);return;}
#ifdef KCF_TOOL_HAS_NETWORK_DIAGNOSTICS
            net::NetworkDiagnosticsClient client(discovery,config);
            std::map<std::string,CachedDiagnostics> cache;std::string cursor;
#endif
#ifdef KCF_TOOL_HAS_REMOTE_CONTROL
            std::shared_ptr<RemoteExecutor> executor;
            if(!options.host_id.empty()){executor=std::make_shared<RemoteExecutor>(discovery,config);std::lock_guard<std::mutex> lock(mutex_);remote_=executor;}
#endif
            for(;;){
                const auto tick=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);
                {std::lock_guard<std::mutex> lock(mutex_);if(stop_)break;}
                auto peers=discovery.GetPeers();
#ifdef KCF_TOOL_HAS_NETWORK_DIAGNOSTICS
                Reconcile(cache,peers,config);
                // One bounded query per 100 ms tick. A rotating Host cursor
                // prevents starvation; ~1 s per Host where capacity permits.
                std::sort(peers.begin(),peers.end(),[](const auto& a,const auto& b){return Key(a.host)<Key(b.host);});
                auto selected=peers.end();
                for(auto it=peers.begin();it!=peers.end();++it)if(Eligibility(*it,config)==DiagnosticsState::PENDING && cache[Key(it->host)].next<=Clock::now()){
                    if(selected==peers.end())selected=it;
                    if(Key(it->host)>cursor){selected=it;break;}
                }
                if(selected!=peers.end() && !discovery.GetLastError()){
                    const auto target=*selected;cursor=Key(target.host);
                    net::NetworkDiagnosticsSnapshot raw;
                    const int code=client.Query(target.host,raw,std::chrono::milliseconds(80));
                    const auto observed=Clock::now();
                    peers=discovery.GetPeers();Reconcile(cache,peers,config);
                    const auto current=std::find_if(peers.begin(),peers.end(),[&](const auto& p){return p.host==target.host;});
                    if(current!=peers.end() && current->boot_id==target.boot_id && current->session_id==target.session_id && Eligibility(*current,config)==DiagnosticsState::PENDING){
                        auto& entry=cache[cursor];entry.next=Clock::now()+std::chrono::seconds(1);
                        entry.value={};entry.value.gateway_host=cursor;entry.value.gateway_boot=Key(target.boot_id);entry.value.gateway_session=target.session_id;
                        if(code){entry.value.status=code==-ETIMEDOUT?DiagnosticsState::TIMEOUT:DiagnosticsState::ERROR;entry.value.error=code;}
                        else entry.value=ConvertDiagnostics(raw);
                        entry.rates.Observe(entry.value,observed);
                    }
                }
#endif
                auto next=Convert(peers,discovery.GetLastError());next.observer_host=Key(config.host);
#ifdef KCF_TOOL_HAS_REMOTE_CONTROL
                next.remote_control_available=executor&&!executor->StartError();
                if(executor&&executor->StartError())next.status+=" | Remote client start failed: "+std::to_string(executor->StartError());
#endif
#ifdef KCF_TOOL_HAS_NETWORK_DIAGNOSTICS
                next.route_status="Gateway diagnostics available in backend snapshot; no inferred network edges";
                for(auto& host:next.hosts)host.diagnostics=cache.at(host.id).value;
#endif
                std::unique_lock<std::mutex> lock(mutex_);snapshot_=std::move(next);
                if(wake_.wait_until(lock,tick,[this]{return stop_;}))break;
            }
#ifdef KCF_TOOL_HAS_REMOTE_CONTROL
            if(executor)executor->Stop();
#endif
            discovery.Stop();
        } catch(...) {std::lock_guard<std::mutex> lock(mutex_);snapshot_.hosts.clear();snapshot_.status="Discovery worker failed";}
    }
    std::mutex mutex_;std::condition_variable wake_;bool stop_{false};
    NetworkSnapshot snapshot_;
#ifdef KCF_TOOL_HAS_REMOTE_CONTROL
    std::weak_ptr<RemoteExecutor> remote_;
#endif
    std::thread worker_;
};
int KcfBackend::StartNetwork(const NetworkOptions& options) {
    std::lock_guard<std::mutex> lock(network_mutex_);if(network_)return -EALREADY;network_options_=options;
    try{network_=std::make_shared<NetworkObserver>(options);return 0;}catch(...){return -EAGAIN;}
}
void KcfBackend::StopNetwork(){std::shared_ptr<NetworkObserver> old;{std::lock_guard<std::mutex> lock(network_mutex_);old=std::move(network_);}old.reset();}
NetworkSnapshot KcfBackend::GetNetworkSnapshot(){std::lock_guard<std::mutex> lock(network_mutex_);return network_?network_->Snapshot():NetworkSnapshot{};}
std::future<RemoteReply> KcfBackend::SubmitRemote(const RemoteCommand& c){std::lock_guard<std::mutex> lock(network_mutex_);return network_?network_->Submit(c):RemoteUnavailable(-ENOTCONN);}
NetworkOptions KcfBackend::GetNetworkOptions() const {std::lock_guard<std::mutex> lock(network_mutex_);return network_options_;}
int KcfBackend::ConfigureNetwork(const NetworkOptions& options,bool enabled){StopNetwork();{std::lock_guard<std::mutex> lock(network_mutex_);network_options_=options;}return enabled?StartNetwork(options):0;}

}
#else
namespace kcf_tool {
int KcfBackend::ConfigureNetwork(const NetworkOptions&,bool){return -ENOTSUP;}
NetworkOptions KcfBackend::GetNetworkOptions() const{return {};}
std::future<RemoteReply> KcfBackend::SubmitRemote(const RemoteCommand&){return RemoteUnavailable();}
int KcfBackend::StartNetwork(const NetworkOptions&){return -ENOTSUP;}
void KcfBackend::StopNetwork(){}
NetworkSnapshot KcfBackend::GetNetworkSnapshot(){return {};}
}
#endif
