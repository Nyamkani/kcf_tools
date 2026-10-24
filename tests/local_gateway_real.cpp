#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf/network/peer_discovery.hpp"
#include <filesystem>
#include <thread>
#include <cassert>
#include <spawn.h>
#include <sys/wait.h>
#include <iostream>
extern char** environ;
using namespace kcf_tool;using namespace std::chrono_literals;namespace n=kcf::network;
template<class F>void Until(F f){auto end=std::chrono::steady_clock::now()+10s;while(!f()){assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(20ms);}}
int main(int argc,char** argv){assert(argc==2);char dir[]="/tmp/tool-gateway-live-XXXXXX";assert(mkdtemp(dir));std::string root=dir,socket=root+"/gateway.sock";
 auto start=[&]{std::vector<std::string> a{argv[1],"--state-dir",root,"--lock-file",root+"/gateway.lock","--ipc-socket",socket,"--interface","lo","--allow-loopback","--discovery-port","48651","--control-port","49001"};std::vector<char*> av;for(auto& s:a)av.push_back(s.data());av.push_back(nullptr);pid_t pid;assert(!posix_spawn(&pid,av[0],nullptr,nullptr,av.data(),environ));return pid;};
 KcfBackend backend;assert(!backend.ConfigureLocalGateway(socket,true));assert(!backend.StartNetwork({"127.0.0.1",48651}));
 Until([&]{return backend.GetLocalGatewaySnapshot().connection==LocalGatewayConnection::ERROR;});auto pid=start();
 Until([&]{auto s=backend.GetLocalGatewaySnapshot();return s.connection==LocalGatewayConnection::READY&&s.state=="RUNNING";});auto first=backend.GetLocalGatewaySnapshot();
 n::DiscoveryConfig dc;dc.host.bytes[0]=77;dc.boot_id.bytes[0]=78;dc.interface_address="127.0.0.1";dc.port=48651;dc.announce_interval=50ms;
 n::NetworkEndpointMetadata e;e.identity={{dc.host,dc.boot_id,getpid(),1},1};e.host_name="remote";e.element_name="element";e.name="/local/ipc/peer";e.value_type={1,2,1,8};n::PeerDiscovery peer;assert(!peer.Start(dc,{e}));
 std::uint64_t revision=0;Until([&]{for(auto& p:backend.GetLocalGatewaySnapshot().peers)if(p.host.substr(0,2)=="4d"&&p.metadata_ready&&p.endpoints.size()==1){revision=p.metadata_revision;return true;}return false;});
 assert(!peer.UpdateMetadata({}));Until([&]{for(auto& p:backend.GetLocalGatewaySnapshot().peers)if(p.host.substr(0,2)=="4d"&&p.metadata_ready&&p.endpoints.empty()&&p.metadata_revision!=revision)return true;return false;});
 int status;kill(pid,SIGTERM);assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&!WEXITSTATUS(status));Until([&]{return backend.GetLocalGatewaySnapshot().connection==LocalGatewayConnection::ERROR;});
 assert(backend.GetLocalGatewaySnapshot().peers.empty());Until([&]{for(auto& h:backend.GetNetworkSnapshot().hosts)if(h.id.substr(0,2)=="4d"&&h.state=="ONLINE")return true;return false;});
 assert(backend.GetNetworkSnapshot().observer_host!=first.host);pid=start();Until([&]{auto s=backend.GetLocalGatewaySnapshot();return s.connection==LocalGatewayConnection::READY&&s.state=="RUNNING"&&s.host==first.host&&s.session!=first.session;});
 backend.ConfigureLocalGateway(socket,false);assert(backend.GetLocalGatewaySnapshot().connection==LocalGatewayConnection::DISABLED);backend.StopNetwork();peer.Stop();kill(pid,SIGTERM);assert(waitpid(pid,&status,0)==pid);std::filesystem::remove_all(root);
 std::cout<<"Real Gateway session/revision/restart, independent Direct Observer and identity isolation PASS\n";
}
