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
int main(){
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
    const auto key=s.hosts[0].endpoints[0].id;
    assert(key!=s.hosts[1].endpoints[0].id);assert(s.hosts[0].compatibility=="COMPATIBLE");
    assert(s.hosts[0].framework_version=="5.1.0" && s.hosts[0].network_version=="1.0");
    assert(s.hosts[0].capabilities.find("REMOTE_TOPIC")!=std::string::npos);
    assert(s.hosts[0].endpoints[0].diagnostic.find("TYPE_MISMATCH")!=std::string::npos);
    const auto revision=s.hosts[0].revision,session=s.hosts[0].session;
    e.element_name="updated";assert(first.UpdateMetadata({e,parameter,service,action})==0);
    assert(Wait([&]{auto s=tool.GetNetworkSnapshot();return s.hosts[0].revision!=revision && s.hosts[0].metadata_ready && s.hosts[0].endpoints[0].element=="updated";}));
    first.Stop();
    assert(Wait([&]{auto s=tool.GetNetworkSnapshot();return s.hosts[0].state=="LOST" && s.hosts[0].endpoints.empty();}));
    a.profile.network.major=2;assert(first.Start(a,{e})==0);
    assert(Wait([&]{auto s=tool.GetNetworkSnapshot();return s.hosts[0].state=="ONLINE" && s.hosts[0].session!=session && s.hosts[0].compatibility=="NETWORK_MAJOR_MISMATCH";}));
    first.Stop();second.Stop();tool.StopNetwork();tool.StopNetwork();
    assert(tool.GetNetworkSnapshot().hosts.empty());
    // Startup errors are asynchronous and isolated from Local introspection.
    assert(tool.StartNetwork({"not-an-ip",port})==0);
    assert(Wait([&]{return tool.GetNetworkSnapshot().status.find("failed")!=std::string::npos;}));
    tool.StopNetwork();
}
