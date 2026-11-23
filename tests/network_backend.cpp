#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf/network/peer_discovery.hpp"
#include <cassert>
#include <chrono>
#include <functional>
#include <thread>
#include <unistd.h>
using namespace kcf_tool;
namespace n=kcf::network;
using namespace std::chrono_literals;
bool Wait(const std::function<bool()>& test){for(int i=0;i<100;++i){if(test())return true;std::this_thread::sleep_for(100ms);}return false;}
// Internal pure converter: deterministic partial-metadata/error states without
// changing Discovery timing or synthesizing the Network wire protocol.
namespace kcf_tool::detail {
NetworkSnapshot ConvertNetworkPeers(const std::vector<n::PeerInfo>&,int);
}
void SnapshotConversion(){
    n::PeerInfo peer;peer.host.bytes[0]=1;peer.boot_id.bytes[0]=2;
    peer.profile.capabilities=n::KNOWN_CAPABILITIES;
    peer.session_id=10;peer.metadata_revision=1;peer.metadata_ready=true;
    n::NetworkEndpointMetadata e;e.identity={{peer.host,peer.boot_id,100,123},1};
    e.element_name="element";e.name="/same";e.value_type={1,2,1,4};e.scope=n::NetworkScope::REMOTE;
    peer.endpoints={e};peer.last_complete_snapshot={peer.endpoints,1,true,false};
    auto convert=[&]{return detail::ConvertNetworkPeers({peer},0).hosts.at(0);};
    auto host=convert();assert(host.metadata_ready&&host.endpoints.size()==1);
    assert(host.last_complete_snapshot.valid&&!host.last_complete_snapshot.stale&&host.last_complete_snapshot.revision==1);
    assert(host.last_complete_snapshot.endpoints[0].id==host.endpoints[0].id);
    assert(host.last_complete_snapshot.endpoints[0].session==10);
    peer.metadata_revision=2;peer.metadata_ready=false;peer.endpoints.clear();peer.last_complete_snapshot.stale=true;
    host=convert();assert(!host.metadata_ready&&host.endpoints.empty()&&host.revision==2);
    assert(host.last_complete_snapshot.valid&&host.last_complete_snapshot.stale&&host.last_complete_snapshot.revision==1&&host.last_complete_snapshot.endpoints.size()==1);
    // Stale publishers must not enter pairwise compatibility checks.
    auto other=peer;other.host.bytes[0]=3;other.metadata_ready=true;other.last_complete_snapshot={};
    auto subscriber=e;subscriber.identity.runtime.host=other.host;subscriber.role=n::EndpointRole::CONSUMER;subscriber.value_type.type_id=2;
    assert(n::Valid(e)&&n::Valid(subscriber));
    other.endpoints={subscriber};auto pair=detail::ConvertNetworkPeers({peer,other},0);
    assert(pair.hosts[0].endpoints.empty()&&pair.hosts[1].endpoints[0].diagnostic.empty());
    assert(pair.hosts[0].last_complete_snapshot.endpoints[0].diagnostic.empty());
    peer.metadata_ready=true;peer.endpoints={e};
    assert(detail::ConvertNetworkPeers({peer,other},0).hosts[1].endpoints[0].diagnostic.find("TYPE_MISMATCH")!=std::string::npos);
    auto added=e;added.identity.registration_id=2;added.name="/added";
    peer.endpoints={e,added};peer.last_complete_snapshot={peer.endpoints,2,true,false};
    host=convert();assert(host.endpoints.size()==2&&host.last_complete_snapshot.endpoints.size()==2&&!host.last_complete_snapshot.stale);
    peer.metadata_revision=3;peer.endpoints={added};peer.last_complete_snapshot={peer.endpoints,3,true,false};
    host=convert();assert(host.endpoints.size()==1&&host.last_complete_snapshot.endpoints[0].name=="/added");
    peer.metadata_revision=UINT64_MAX;peer.endpoints.clear();peer.last_complete_snapshot={{},UINT64_MAX,true,false};
    host=convert();assert(host.metadata_ready&&host.endpoints.empty()&&host.last_complete_snapshot.valid&&host.last_complete_snapshot.endpoints.empty()&&host.last_complete_snapshot.revision==UINT64_MAX);
    peer.state=n::PeerState::LOST;
    host=convert();assert(!host.metadata_ready&&host.endpoints.empty()&&!host.last_complete_snapshot.valid&&host.last_complete_snapshot.endpoints.empty());
    // Core resets the display snapshot on boot/session change; conversion must
    // never fill the new generation from a previously converted host.
    peer.state=n::PeerState::ONLINE;peer.session_id=11;peer.metadata_ready=false;peer.last_complete_snapshot={};
    host=convert();assert(host.session==11&&!host.last_complete_snapshot.valid&&host.endpoints.empty());
    peer.boot_id.bytes[0]=4;host=convert();assert(host.boot_id!=pair.hosts[0].boot_id&&!host.last_complete_snapshot.valid);
    assert(detail::ConvertNetworkPeers({peer,other},-EIO).hosts.empty());
}
int main(){
    SnapshotConversion();
    const auto port=static_cast<std::uint16_t>(39000+getpid()%10000);
    n::DiscoveryConfig a;a.host.bytes[0]=1;a.boot_id.bytes[0]=2;a.interface_address="127.0.0.1";a.port=port;
    a.announce_interval=100ms;a.profile.capabilities=n::KNOWN_CAPABILITIES;
    n::DiscoveryConfig b=a;b.host.bytes[0]=3;
    n::NetworkEndpointMetadata e;e.identity={{a.host,a.boot_id,100,123},100};e.application={a.host,a.boot_id,50,99};
    e.application_name="same-app";e.element_name="same-element";e.name="/same-topic";e.host_name="same-host-label";
    e.scope=n::NetworkScope::REMOTE;e.value_type={1,2,1,4};
    auto f=e;f.identity.runtime.host=b.host;f.application.host=b.host;f.role=n::EndpointRole::CONSUMER;f.value_type.type_id=2;
    auto parameter=e;parameter.identity.registration_id=101;parameter.kind=n::EndpointKind::PARAMETER;
    auto service=e;service.identity.registration_id=102;service.kind=n::EndpointKind::SERVICE;
    service.value_type={};service.request_type=service.response_type={1,2,1,4};
    auto action=service;action.identity.registration_id=103;action.kind=n::EndpointKind::ACTION;action.feedback_type={1,2,1,4};
    n::PeerDiscovery first,second;assert(first.Start(a,{e,parameter,service,action})==0);assert(second.Start(b,{f})==0);
    KcfBackend tool;assert(tool.StartNetwork({"127.0.0.1",port})==0);assert(tool.StartNetwork()==-EALREADY);
    assert(Wait([&]{auto s=tool.GetNetworkSnapshot();return s.hosts.size()==2 && s.hosts[0].metadata_ready && s.hosts[1].metadata_ready;}));
    auto s=tool.GetNetworkSnapshot();assert(s.hosts[0].id!=s.hosts[1].id);
    assert(s.hosts[0].endpoints.size()==4);
    assert(s.hosts[0].endpoints[1].kind=="P" && s.hosts[0].endpoints[1].role=="OWNER");
    assert(s.hosts[0].endpoints[2].kind=="S" && s.hosts[0].endpoints[2].role=="SERVER");
    assert(s.hosts[0].endpoints[3].kind=="A" && s.hosts[0].endpoints[3].role=="SERVER");
    // Local refresh replaces its registry cache without losing this observer.
    tool.Refresh();assert(tool.GetNetworkSnapshot().hosts[0].session==s.hosts[0].session);
    assert(s.hosts[0].last_complete_snapshot.valid&&!s.hosts[0].last_complete_snapshot.stale);
    assert(s.hosts[0].last_complete_snapshot.revision==s.hosts[0].revision&&s.hosts[0].last_complete_snapshot.endpoints.size()==4);
    const auto key=s.hosts[0].endpoints[0].id;
    assert(key!=s.hosts[1].endpoints[0].id);assert(s.hosts[0].compatibility=="COMPATIBLE");
    const auto& version=a.profile.framework;
    const auto expected_version=std::to_string(version.major)+"."+std::to_string(version.minor)+"."+std::to_string(version.patch);
    assert(s.hosts[0].framework_version==expected_version && s.hosts[0].network_version=="1.0");
    assert(s.hosts[0].capabilities.find("REMOTE_TOPIC")!=std::string::npos);
    assert(s.hosts[0].endpoints[0].diagnostic.find("TYPE_MISMATCH")!=std::string::npos);
    const auto revision=s.hosts[0].revision,session=s.hosts[0].session;
    e.element_name="updated";assert(first.UpdateMetadata({e,parameter,service,action})==0);
    assert(Wait([&]{auto s=tool.GetNetworkSnapshot();return s.hosts[0].revision!=revision && s.hosts[0].metadata_ready && s.hosts[0].endpoints[0].element=="updated";}));
    assert(tool.GetNetworkSnapshot().hosts[0].last_complete_snapshot.endpoints[0].element=="updated");
    assert(first.UpdateMetadata({})==0);
    assert(Wait([&]{auto h=tool.GetNetworkSnapshot().hosts[0];return h.metadata_ready&&h.endpoints.empty()&&h.last_complete_snapshot.valid&&h.last_complete_snapshot.endpoints.empty();}));
    first.Stop();
    assert(Wait([&]{auto s=tool.GetNetworkSnapshot();return s.hosts[0].state=="LOST" && s.hosts[0].endpoints.empty() && !s.hosts[0].last_complete_snapshot.valid;}));
    a.boot_id.bytes[0]=5;e.identity.runtime.boot_id=a.boot_id;e.application.boot_id=a.boot_id;a.profile.network.major=2;assert(first.Start(a,{e})==0);
    assert(Wait([&]{auto s=tool.GetNetworkSnapshot();return s.hosts[0].state=="ONLINE" && s.hosts[0].session!=session && s.hosts[0].compatibility=="NETWORK_MAJOR_MISMATCH" && !s.hosts[0].last_complete_snapshot.valid;}));
    first.Stop();second.Stop();tool.StopNetwork();tool.StopNetwork();
    assert(tool.GetNetworkSnapshot().hosts.empty());
    // Startup errors are asynchronous and isolated from Local introspection.
    assert(tool.StartNetwork({"not-an-ip",port})==0);
    assert(Wait([&]{return tool.GetNetworkSnapshot().status.find("failed")!=std::string::npos;}));
    assert(tool.GetNetworkSnapshot().hosts.empty());
    tool.StopNetwork();
}
