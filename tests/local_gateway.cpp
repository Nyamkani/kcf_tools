#include "kcf_tool/backend/local_gateway.hpp"
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <filesystem>
#include <atomic>
#include <cassert>
#include <thread>
#include <iostream>
using namespace kcf_tool;using namespace std::chrono_literals;using J=nlohmann::json;
std::string Id(char c){return std::string(32,c);}
J Type(){return {{"type_id","18446744073709551615"},{"schema_id","2"},{"encoding_id",1},{"payload_size",8}};}
J Runtime(){return {{"host",Id('3')},{"boot",Id('4')},{"pid",123},{"start_ticks","18446744073709551615"}};}
J Status(){return {{"state","RUNNING"},{"host",Id('1')},{"boot",Id('2')},{"interface","lo"},{"ipv4","127.0.0.1"},{"interface_index",1},{"session","7"},{"last_error",0}};}
J Peers(){auto app=Runtime();app["host"]=app["boot"]=std::string(32,'0');app["pid"]=0;app["start_ticks"]="0";
 J e={{"runtime",Runtime()},{"registration_id","99"},{"application",app},{"host_name","host"},{"application_name",""},{"element_name","e"},{"name","/topic"},{"kind",1},{"role",1},{"scope",1},{"value_type",Type()},{"request_type",Type()},{"response_type",Type()},{"feedback_type",Type()},{"groups",J::array()}};
 return {{"peers",J::array({{{"host",Id('3')},{"boot",Id('4')},{"session","11"},{"state","ONLINE"},{"address","127.0.0.1"},{"metadata_port",39001},{"metadata_revision","5"},{"metadata_ready",true},{"endpoints",J::array({e})}}})}};
}
struct Probe {
 const std::optional<J> peers_response;
 std::string path;int listener;std::atomic<bool> stop{false};std::atomic<int> mode{0},state{0},revision{5},requests{0};std::thread worker;
 explicit Probe(std::string p,std::optional<J> peers=std::nullopt):peers_response(std::move(peers)),path(p){listener=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);assert(listener>=0);sockaddr_un a{};a.sun_family=AF_UNIX;std::copy(path.begin(),path.end(),a.sun_path);assert(!bind(listener,(sockaddr*)&a,sizeof(a))&&!listen(listener,8));worker=std::thread([this]{Run();});}
 ~Probe(){stop=true;worker.join();close(listener);unlink(path.c_str());}
 void Run(){while(!stop){pollfd p{listener,POLLIN,0};if(poll(&p,1,20)<=0)continue;int fd=accept4(listener,nullptr,nullptr,SOCK_CLOEXEC);assert(fd>=0);timeval tv{1,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));char h[16];if(recv(fd,h,16,MSG_WAITALL)!=16){close(fd);continue;}++requests;int m=mode,op=(unsigned char)h[11];auto j=op==1?Status():Peers();
 if(op==1){const char* states[]={"RUNNING","WAITING_NETWORK","REBINDING","ERROR"};j["state"]=states[state];if(state)j["session"]="0";if(m==10)j["session"]=std::to_string(requests.load());}
 else {j["peers"][0]["metadata_revision"]=std::to_string(revision.load());if(m==11)j["peers"]=J::array();}
 if(op==2&&peers_response)j=*peers_response;
 if(m==5)j.erase(op==1?"host":"peers");if(m==6&&op==1)j["session"]="18446744073709551616";
 std::string body=m==4?"not JSON":j.dump();unsigned size=body.size();if(m==1)size=1048577;for(int i=0;i<4;++i)h[i]=size>>(24-i*8);if(m==2)h[9]=2;if(m==3)h[4]='X';
 if(m==8){while(!stop&&mode==8)std::this_thread::sleep_for(10ms);close(fd);continue;}
 if(m==12){body.clear();for(int i=0;i<4;++i){h[i]=0;h[12+i]=std::uint32_t(-E2BIG)>>(24-i*8);}}
 std::string frame(h,16);frame+=body;if(m==7)frame.resize(8);
 for(char c:frame)if(send(fd,&c,1,MSG_NOSIGNAL)!=1)break;
 close(fd);
 }}
};
template<class F>void Until(F f){auto end=std::chrono::steady_clock::now()+5s;while(!f()){assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(10ms);}}
std::size_t Fds(){return std::distance(std::filesystem::directory_iterator("/proc/self/fd"),std::filesystem::directory_iterator());}
// Each case uses the real framed Unix-socket path and a fresh client, avoiding
// stale results from earlier cycles. Snapshot data never replaces current data.
template<class F>void SnapshotCase(const std::string& path,J peers,F check,int error=0){
 Probe server(path,std::move(peers));LocalGatewayClient client(path);
 Until([&]{auto c=client.Snapshot().connection;return c==LocalGatewayConnection::READY||c==LocalGatewayConnection::ERROR;});
 auto s=client.Snapshot();if(error){assert(s.connection==LocalGatewayConnection::ERROR&&s.error==error&&s.peers.empty());}
 else {assert(s.connection==LocalGatewayConnection::READY&&s.peers.size()==1);check(s.peers[0]);}
}
void SnapshotTests(const std::string& path){
 auto response=Peers();auto& peer=response["peers"][0];
 auto endpoint=peer["endpoints"][0];endpoint["name"]="/last-topic";
 peer["last_complete_snapshot"]={{"valid",true},{"stale",false},{"revision","18446744073709551615"},{"endpoints",J::array({endpoint})}};
 SnapshotCase(path,response,[](const auto& p){const auto& s=p.last_complete_snapshot;assert(p.metadata_ready&&p.metadata_revision==5&&p.endpoints[0].name=="/topic");assert(s&&s->valid&&!s->stale&&s->revision==UINT64_MAX&&s->endpoints.size()==1&&s->endpoints[0].name=="/last-topic");assert(s->endpoints[0].identity.registration_id==99&&s->endpoints[0].identity.start_ticks==UINT64_MAX&&s->endpoints[0].value_type.type_id==UINT64_MAX);});
 peer["metadata_ready"]=false;peer["endpoints"]=J::array();peer["last_complete_snapshot"]["stale"]=true;
 SnapshotCase(path,response,[](const auto& p){assert(!p.metadata_ready&&p.endpoints.empty()&&p.last_complete_snapshot->stale&&p.last_complete_snapshot->endpoints.size()==1);});
 peer["last_complete_snapshot"]["endpoints"]=J::array();
 for(bool stale:{false,true}){peer["last_complete_snapshot"]["stale"]=stale;SnapshotCase(path,response,[&](const auto& p){assert(p.last_complete_snapshot->valid&&p.last_complete_snapshot->stale==stale&&p.last_complete_snapshot->endpoints.empty());});}
 peer["last_complete_snapshot"]={{"valid",false},{"stale",false},{"revision","0"},{"endpoints",J::array()}};
 SnapshotCase(path,response,[](const auto& p){assert(p.last_complete_snapshot&&!p.last_complete_snapshot->valid&&!p.last_complete_snapshot->stale&&!p.last_complete_snapshot->revision&&p.last_complete_snapshot->endpoints.empty());});
 const J good={{"valid",true},{"stale",false},{"revision","7"},{"endpoints",J::array({endpoint})}};
 auto reject=[&](J snapshot){auto bad=Peers();bad["peers"][0]["last_complete_snapshot"]=std::move(snapshot);SnapshotCase(path,bad,[](const auto&){},-EPROTO);};
 for(const char* field:{"valid","stale","revision","endpoints"}){auto bad=good;bad.erase(field);reject(bad);}
 reject(nullptr);reject(J::array());
 for(const J& value:std::vector<J>{nullptr,true,7,-1,1.5,"","-1","+1","1x","18446744073709551616"}){auto bad=good;bad["revision"]=value;reject(bad);}
 for(const char* field:{"valid","stale"}){auto bad=good;bad[field]=1;reject(bad);}
 {auto bad=good;bad["endpoints"]=J::object();reject(bad);}
 {auto bad=good;bad["endpoints"].push_back(endpoint);reject(bad);}
 {auto bad=good;bad["endpoints"]=J::array();for(int i=0;i<65;++i)bad["endpoints"].push_back(endpoint);reject(bad);}
 for(const char* field:{"host","boot","pid","start_ticks"}){auto bad=good;bad["endpoints"][0]["runtime"][field]=field==std::string("pid")?J(0):J("0");reject(bad);}
 for(const char* field:{"host","boot"}){auto bad=good;bad["endpoints"][0]["runtime"][field]=Id('5');reject(bad);}
 for(const char* field:{"registration_id","name","kind","role","scope","value_type","application","groups"}){auto bad=good;bad["endpoints"][0][field]=nullptr;reject(bad);}
 std::cout<<"Optional display snapshot legacy/valid/stale/empty/invalid/uint64/endpoint validation PASS\n";
}
int main(){char tmp[]="/tmp/tool-local-ipc-XXXXXX";assert(mkdtemp(tmp));std::string path=std::string(tmp)+"/gateway.sock";auto baseline=Fds();{
 LocalGatewayClient client(path);Until([&]{return client.Snapshot().connection==LocalGatewayConnection::ERROR;});assert(client.Snapshot().peers.empty());
 {Probe server(path);Until([&]{return client.Snapshot().connection==LocalGatewayConnection::READY;});auto s=client.Snapshot();assert(!s.peers[0].last_complete_snapshot);assert(s.session==7&&s.peers[0].endpoints[0].identity.start_ticks==UINT64_MAX&&s.peers[0].endpoints[0].value_type.type_id==UINT64_MAX);
 for(int i=1;i<4;++i){server.state=i;const char* names[]={"RUNNING","WAITING_NETWORK","REBINDING","ERROR"};Until([&]{return client.Snapshot().state==names[i];});}server.state=0;
 server.revision=6;Until([&]{return client.Snapshot().connection==LocalGatewayConnection::READY&&client.Snapshot().peers[0].metadata_revision==6;});
 for(int mode:{1,2,3,4,5,6,7,10,12}){server.mode=mode;Until([&]{return client.Snapshot().connection==LocalGatewayConnection::ERROR;});s=client.Snapshot();if(mode==12)assert(s.error==-E2BIG);if(mode==1)assert(s.error==-EPROTO);assert(s.host.empty()&&s.peers.empty()&&!s.observed_at_unix_ms);server.mode=0;Until([&]{return client.Snapshot().connection==LocalGatewayConnection::READY;});}
 server.mode=11;Until([&]{return client.Snapshot().connection==LocalGatewayConnection::READY&&client.Snapshot().peers.empty();});
 server.mode=8;Until([&]{return client.Snapshot().error==-ETIMEDOUT;});server.mode=0;Until([&]{return client.Snapshot().connection==LocalGatewayConnection::READY;});int requests=server.requests;server.mode=8;Until([&]{return server.requests>requests;});auto before=std::chrono::steady_clock::now();client.Stop();assert(std::chrono::steady_clock::now()-before<500ms);server.mode=0;
 }
 }
 SnapshotTests(path);
 assert(Fds()==baseline);std::filesystem::remove_all(tmp);std::cout<<"Local IPC status/peers/states/revision/fragmentation/invalid frames+JSON/session race/timeout/Stop/FD PASS\n";return 0;
}
