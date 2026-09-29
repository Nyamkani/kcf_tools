#include "kcf_tool/backend/kcf_backend.hpp"
#include <cerrno>
#ifdef KCF_TOOL_HAS_NETWORK
#include "kcf/network/peer_discovery.hpp"
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
        "TARGET_ENDPOINT","TARGET_HOST","TARGET_APPLICATION","TARGET_GROUP","TARGET_ALL"};
    std::string out;
    for(unsigned i=0;i<10;++i)if(bits & (1ull<<i)){if(!out.empty())out+=", ";out+=names[i];}
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
            RemoteEndpoint endpoint;endpoint.id=Key(e.identity);endpoint.runtime_id=Key(e.identity.runtime);
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
}
class NetworkObserver {
public:
    explicit NetworkObserver(NetworkOptions options):worker_([this,options]{Run(options);}) {}
    ~NetworkObserver(){ {std::lock_guard<std::mutex> lock(mutex_);stop_=true;} wake_.notify_all();worker_.join(); }
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
            config.interface_address=options.interface_address;config.port=options.port;
            const int result=discovery.Start(config);
            if(result){std::lock_guard<std::mutex> lock(mutex_);snapshot_.status="Discovery start failed: "+std::to_string(result);return;}
            for(;;){
                auto next=Convert(discovery.GetPeers(),discovery.GetLastError());
                std::unique_lock<std::mutex> lock(mutex_);snapshot_=std::move(next);
                if(wake_.wait_for(lock,std::chrono::milliseconds(100),[this]{return stop_;}))break;
            }
            discovery.Stop();
        } catch(...) {std::lock_guard<std::mutex> lock(mutex_);snapshot_.hosts.clear();snapshot_.status="Discovery worker failed";}
    }
    std::mutex mutex_;std::condition_variable wake_;bool stop_{false};
    NetworkSnapshot snapshot_;std::thread worker_;
};
int KcfBackend::StartNetwork(const NetworkOptions& options) {
    if(network_)return -EALREADY;
    try{network_=std::make_shared<NetworkObserver>(options);return 0;}catch(...){return -EAGAIN;}
}
void KcfBackend::StopNetwork(){network_.reset();}
NetworkSnapshot KcfBackend::GetNetworkSnapshot(){return network_?network_->Snapshot():NetworkSnapshot{};}
}
#else
namespace kcf_tool {
int KcfBackend::StartNetwork(const NetworkOptions&){return -ENOTSUP;}
void KcfBackend::StopNetwork(){}
NetworkSnapshot KcfBackend::GetNetworkSnapshot(){return {};}
}
#endif
