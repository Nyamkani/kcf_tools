#include "kcf_tool/backend/kcf_backend.hpp"
#include "../src/backend/network_rates.hpp"
#include <cmath>
#include "kcf/network/control_node.hpp"
#include "kcf/network/network_diagnostics.hpp"
#include "kcf/introspection/detail/runtime_registry.hpp"
#include "kcf/ipc/publisher.hpp"
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;
namespace n=kcf::network;
using namespace kcf_tool;
struct Sample {std::uint64_t value;};
namespace kcf {template<>struct TypeDescriptorTraits<Sample>{static constexpr bool defined=true;static TypeDescriptor Get(){auto d=MakeTypeDescriptor<Sample>("tool.diagnostics.sample.v1");d.field_count=1;d.fields[0]=MakeField<Sample,std::uint64_t>("value",0);return d;}};}
n::HostIdentity Id(unsigned char h){n::HostIdentity id;id.bytes[0]=h;return id;}
std::string Key(unsigned h){const char hex[]="0123456789abcdef";std::string s;s+=hex[h>>4];s+=hex[h&15];return s+std::string(30,'0');}
void Wait(const std::function<bool()>& f){const auto end=std::chrono::steady_clock::now()+12s;while(!f()){assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(20ms);}}
std::uint16_t Port(int kind=SOCK_DGRAM,int* keep=nullptr){int fd=socket(AF_INET,kind|SOCK_CLOEXEC,0);assert(fd>=0);sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);assert(!bind(fd,reinterpret_cast<sockaddr*>(&a),sizeof(a)));socklen_t size=sizeof(a);assert(!getsockname(fd,reinterpret_cast<sockaddr*>(&a),&size));if(keep)*keep=fd;else close(fd);return ntohs(a.sin_port);}
n::DiscoveryConfig Config(unsigned char host,unsigned port){n::DiscoveryConfig c;c.host=Id(host);c.boot_id=Id(42);c.interface_address="127.0.0.1";c.port=port;c.announce_interval=50ms;return c;}
RemoteNetworkDiagnostics Value(KcfBackend& tool,unsigned host){for(const auto& h:tool.GetNetworkSnapshot().hosts)if(h.id==Key(host))return h.diagnostics;return {};}
bool Read(int fd,void* data,std::size_t size){auto p=static_cast<char*>(data);while(size){pollfd f{fd,POLLIN,0};if(poll(&f,1,500)<=0)return false;auto n=recv(fd,p,size,0);if(n<=0)return false;p+=n;size-=n;}return true;}
bool Frame(int fd,n::ControlFrame& f){std::vector<std::uint8_t> b(n::CONTROL_PREFIX_SIZE);if(!Read(fd,b.data(),b.size()))return false;std::size_t size=0;if(n::ControlFrameSize(b.data(),b.size(),size))return false;b.resize(size);return Read(fd,b.data()+16,size-16)&&!n::DecodeControlFrame(b.data(),b.size(),f);}
void Send(int fd,const n::ControlFrame& f){std::vector<std::uint8_t> b;assert(!n::EncodeControlFrame(f,b));std::size_t offset=0;while(offset<b.size()){auto count=send(fd,b.data()+offset,b.size()-offset,MSG_NOSIGNAL);if(count<=0)return;offset+=count;}}
// Protocol probe only: explicit snapshots test every DTO field and failure state.
struct Probe {
    n::PeerDiscovery discovery;n::DiscoveryConfig config;int listener;std::atomic<bool> stop{false};std::atomic<int> mode{0},requests{0};std::thread worker;
    Probe(unsigned host,unsigned port):config(Config(host,port)){
        config.metadata_port=Port(SOCK_STREAM,&listener);assert(!listen(listener,8));
        config.profile.capabilities|=n::CapabilityBit(n::Capability::NETWORK_DIAGNOSTICS);assert(!discovery.Start(config));
        worker=std::thread([this]{while(!stop){pollfd p{listener,POLLIN,0};if(poll(&p,1,20)<=0)continue;int fd=accept4(listener,nullptr,nullptr,SOCK_CLOEXEC);if(fd<0)continue;
            n::ControlFrame hello,request;if(Frame(fd,hello)){
                hello.source={config.host,config.boot_id,getpid(),1};hello.session_id=discovery.GetSessionId();Send(fd,hello);
                if(Frame(fd,request)){++requests;const int m=mode;
                    if(m==1)std::this_thread::sleep_for(180ms);
                    else {auto response=request;response.operation=n::ControlMessage::DIAGNOSTICS_RESPONSE;response.source=hello.source;response.session_id=hello.session_id;response.timeout_ms=0;
                        n::NetworkDiagnosticsSnapshot s;s.gateway_host=config.host;s.gateway_boot_id=config.boot_id;s.gateway_session=hello.session_id;
                        n::TopicRouteInfo r;r.source={{Id(10),Id(42),100,123},11};r.destination={{Id(11),Id(43),101,124},12};r.source_session=13;r.destination_session=14;r.route_id=15;r.name="/same-topic";r.state=n::TopicRouteState::ACTIVE;r.sending=config.host==Id(10);r.error=-EIO;r.reason=n::CompatibilityReason::TYPE_MISMATCH;r.proxy_recovery=n::ProxyRecoveryState::RECOVERED;
                        r.packets_sent=21;r.packets_received=22;r.samples_published=23;r.dropped=24;r.duplicate=25;r.out_of_order=26;r.missed_sequence=27;r.malformed=28;r.local_samples_skipped=29;
                        s.routes.assign(m==2?100:2,r);s.total_routes=s.routes.size();s.routes[1].route_id=16;s.routes[1].source.registration_id=17;
                        assert(!n::EncodeNetworkDiagnosticsSnapshot(s,response.payload));if(m==3)++response.request_id;Send(fd,response);
                    }
                }
            }close(fd);
        }});
    }
    ~Probe(){stop=true;worker.join();close(listener);discovery.Stop();}
};
void Real(KcfBackend& tool,unsigned port){
    kcf::detail::BeginRuntimeIntrospection(kcf::ExecutionMode::STANDALONE);kcf::Publisher<Sample> publisher;const auto name="/tool_diag_"+std::to_string(getpid());assert(!publisher.Create(name));assert(!publisher.Publish({9}));kcf::detail::UpdateRuntimeIntrospection(kcf::ProcessState::RUNNING,0);
    n::ControlConfig c;c.discovery=Config(1,port);c.diagnostics_enabled=true;c.metadata.runtime_pids={getpid()};c.metadata.remote_topics={name};c.topics.enabled=true;c.topics.port=Port();
    n::PeerDiscovery consumer;auto cc=Config(3,port);cc.interface_address="127.0.0.3";cc.profile.capabilities|=n::CapabilityBit(n::Capability::REMOTE_TOPIC)|n::CapabilityBit(n::Capability::TARGET_ENDPOINT);
    n::NetworkEndpointMetadata e;e.identity={{cc.host,cc.boot_id,getpid(),1},1};e.kind=n::EndpointKind::TOPIC;e.role=n::EndpointRole::CONSUMER;e.scope=n::NetworkScope::REMOTE;e.name=name;e.element_name="diagnostics_consumer";assert(!n::ParameterWireType(kcf::TypeDescriptorTraits<Sample>::Get(),e.value_type));assert(!consumer.Start(cc,{e}));
    n::ControlNode gateway;assert(!gateway.Start(c));Wait([&]{return Value(tool,1).status==DiagnosticsState::VALID;});assert(Value(tool,1).routes.empty());
    n::PeerInfo p;Wait([&]{for(auto& v:consumer.GetPeers())if(v.host==Id(1)&&v.metadata_ready&&!v.endpoints.empty()){p=v;return true;}return false;});
    int udp=socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC,0);sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(c.topics.port);inet_pton(AF_INET,"127.0.0.3",&a.sin_addr);assert(!bind(udp,reinterpret_cast<sockaddr*>(&a),sizeof(a)));
    n::TopicPacket interest;interest.source=p.endpoints[0].identity;interest.destination=e.identity;interest.source_session=p.session_id;interest.destination_session=consumer.GetSessionId();interest.route_id=1;interest.name=name;interest.type=e.value_type;
    std::vector<std::uint8_t> bytes;assert(!n::EncodeTopicPacket(interest,bytes));inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);assert(sendto(udp,bytes.data(),bytes.size(),0,reinterpret_cast<sockaddr*>(&a),sizeof(a))==ssize_t(bytes.size()));
    Wait([&]{auto v=Value(tool,1);return v.status==DiagnosticsState::VALID&&!v.routes.empty()&&v.routes[0].packets_sent>0;});
    auto v=Value(tool,1);auto raw=gateway.GetTopicDiagnostics();assert(v.routes[0].packets_sent==raw.routes[0].packets_sent && v.routes[0].source_session==p.session_id && v.gateway_session==p.session_id && v.observed_at_unix_ms);
    gateway.Stop();Wait([&]{auto v=Value(tool,1);return v.status==DiagnosticsState::LOST&&v.routes.empty()&&!v.observed_at_unix_ms;});
    c.topics.enabled=false;assert(!gateway.Start(c));Wait([&]{auto v=Value(tool,1);return v.status==DiagnosticsState::VALID&&v.gateway_session!=p.session_id&&v.routes.empty();});
    gateway.Stop();consumer.Stop();close(udp);publisher.Close();publisher.Unlink();kcf::detail::EndRuntimeIntrospection(kcf::ProcessState::STOPPED,0);
}
void RateCalculation(){
    using Tracker=kcf_tool::detail::NetworkRateTracker;using Clock=std::chrono::steady_clock;
    const auto start=Clock::time_point{};
    const auto sample=[](std::uint64_t count){RemoteNetworkDiagnostics s;s.status=DiagnosticsState::VALID;
        s.gateway_host="gateway";s.gateway_boot="boot";s.gateway_session=1;
        RemoteRoute r;r.name="/same";r.state=1;r.sending=true;r.source={"source","boot",10,11,12};r.destination={"destination","boot",20,21,22};
        r.source_session=31;r.destination_session=32;r.route_id=33;
        r.packets_sent=count;r.packets_received=2*count;r.samples_published=3*count;r.local_samples_skipped=4*count;
        r.dropped=count;r.duplicate=count;r.out_of_order=count;r.missed_sequence=count;r.malformed=count;s.routes={r};return s;};
    const auto near=[](double a,double b){assert(std::abs(a-b)<1e-9);};
    Tracker tracker;auto s=sample(100);tracker.Observe(s,start);assert(s.routes[0].rates.status==NetworkRateState::MEASURING);
    s=sample(112);tracker.Observe(s,start+240ms);assert(s.routes[0].rates.status==NetworkRateState::MEASURING);
    s=sample(125);tracker.Observe(s,start+500ms);auto rate=s.routes[0].rates;assert(rate.status==NetworkRateState::VALID);
    near(rate.send_hz,50);near(rate.receive_hz,100);near(rate.proxy_publish_hz,150);near(rate.local_skipped_per_second,200);
    assert(rate.delta.dropped==25&&rate.delta.duplicate==25&&rate.delta.out_of_order==25&&rate.delta.missed_sequence==25&&rate.delta.malformed==25);
    assert(s.routes[0].packets_sent==125&&s.routes[0].local_samples_skipped==500); // raw values preserved
    s=sample(250);tracker.Observe(s,start+3s);near(s.routes[0].rates.send_hz,50);near(s.routes[0].rates.interval_seconds,2.5);
    s=sample(250);tracker.Observe(s,start+4s);assert(s.routes[0].rates.status==NetworkRateState::VALID);near(s.routes[0].rates.send_hz,0);
    // Every identity component matters, independently of the identical Topic name.
    std::vector<std::function<void(RemoteNetworkDiagnostics&)>> changes={
        [](auto& v){v.gateway_host+="x";},[](auto& v){v.gateway_boot+="x";},[](auto& v){++v.gateway_session;},
        [](auto& v){v.routes[0].source.host_id+="x";},[](auto& v){v.routes[0].source.boot_id+="x";},
        [](auto& v){++v.routes[0].source.pid;},[](auto& v){++v.routes[0].source.start_ticks;},[](auto& v){++v.routes[0].source.registration_id;},
        [](auto& v){v.routes[0].destination.host_id+="x";},[](auto& v){v.routes[0].destination.boot_id+="x";},
        [](auto& v){++v.routes[0].destination.pid;},[](auto& v){++v.routes[0].destination.start_ticks;},[](auto& v){++v.routes[0].destination.registration_id;},
        [](auto& v){++v.routes[0].source_session;},[](auto& v){++v.routes[0].destination_session;},
        [](auto& v){++v.routes[0].route_id;},[](auto& v){v.routes[0].sending=false;}};
    for(const auto& change:changes){Tracker t;auto a=sample(100);t.Observe(a,start);auto b=sample(150);change(b);t.Observe(b,start+1s);assert(b.routes[0].rates.status==NetworkRateState::MEASURING);}
    for(auto member:{&RemoteRoute::packets_sent,&RemoteRoute::packets_received,&RemoteRoute::samples_published,&RemoteRoute::dropped,&RemoteRoute::duplicate,&RemoteRoute::out_of_order,&RemoteRoute::missed_sequence,&RemoteRoute::malformed,&RemoteRoute::local_samples_skipped}){
        Tracker t;auto a=sample(100);t.Observe(a,start);auto b=sample(150);b.routes[0].*member=0;t.Observe(b,start+1s);assert(b.routes[0].rates.status==NetworkRateState::MEASURING);
    }
    for(auto state:{DiagnosticsState::ERROR,DiagnosticsState::TIMEOUT,DiagnosticsState::LOST,DiagnosticsState::UNSUPPORTED,DiagnosticsState::INCOMPATIBLE}){
        Tracker t;auto a=sample(100);t.Observe(a,start);auto failure=a;failure.status=state;t.Observe(failure,start+1s);assert(failure.routes[0].rates.status==NetworkRateState::INVALID);
        auto b=sample(200);t.Observe(b,start+2s);assert(b.routes[0].rates.status==NetworkRateState::MEASURING);b=sample(250);t.Observe(b,start+3s);near(b.routes[0].rates.send_hz,50);
    }
    Tracker t;auto a=sample(100);auto second=a.routes[0];second.route_id=34;second.sending=false;second.packets_sent=1000;a.routes.push_back(second);t.Observe(a,start);
    auto b=sample(150);second.packets_sent=1200;b.routes.push_back(second);b.status=DiagnosticsState::TRUNCATED;b.truncated=true;b.total_routes=3;t.Observe(b,start+1s);
    near(b.routes[0].rates.send_hz,50);near(b.routes[1].rates.send_hz,200); // same name, distinct route/role
    b=sample(200);b.status=DiagnosticsState::TRUNCATED;b.truncated=true;b.total_routes=3;t.Observe(b,start+2s);near(b.routes[0].rates.send_hz,50);
    b=sample(250);second.packets_sent=1400;b.routes.push_back(second);t.Observe(b,start+3s);
    assert(b.routes[1].rates.status==NetworkRateState::MEASURING);near(b.routes[0].rates.send_hz,50);
    b.routes[0].state=3;t.Observe(b,start+4s);assert(b.routes[0].rates.status==NetworkRateState::INVALID);
    b=sample(350);t.Observe(b,start+5s);assert(b.routes[0].rates.status==NetworkRateState::MEASURING);
    b.routes.clear();t.Observe(b,start+6s);b=sample(450);t.Observe(b,start+7s);assert(b.routes[0].rates.status==NetworkRateState::MEASURING);
    Tracker receive;auto receiver=sample(100);receiver.gateway_host="other";receive.Observe(receiver,start+100ms);
    receiver=sample(200);receiver.gateway_host="other";receive.Observe(receiver,start+2100ms);near(receiver.routes[0].rates.send_hz,50);
    std::cout<<"Network rates: 50 Hz, subsecond/long intervals, zero, full identities, all counter resets, errors, closed/missing/truncated routes and independent clocks PASS\n";
}
int main(){
    RateCalculation();
    const auto port=Port();KcfBackend tool;assert(!tool.StartNetwork({"127.0.0.1",port}));Real(tool,port);
    Probe a(10,port),b(11,port);n::PeerDiscovery unsupported;auto u=Config(12,port);assert(!unsupported.Start(u));
    Wait([&]{return Value(tool,10).status==DiagnosticsState::VALID&&Value(tool,11).status==DiagnosticsState::VALID;});
    auto x=Value(tool,10),y=Value(tool,11);assert(x.routes.size()==2&&y.routes.size()==2 && x.gateway_host!=y.gateway_host && x.routes[0].name==y.routes[0].name && x.routes[0].sending&&!y.routes[0].sending);
    Wait([&]{return Value(tool,10).routes[0].rates.status==NetworkRateState::VALID;});
    auto measured=Value(tool,10);assert(measured.routes[0].rates.send_hz==0 && measured.routes[0].rates.interval_seconds>=0.5);
    for(int i=0;i<5;++i){auto copy=Value(tool,10);if(copy.observed_at_unix_ms==measured.observed_at_unix_ms)
        assert(copy.routes[0].rates.interval_seconds==measured.routes[0].rates.interval_seconds);std::this_thread::sleep_for(20ms);}
    const auto& r=x.routes[0];assert(r.source.host_id==Key(10)&&r.source.boot_id==Key(42)&&r.source.pid==100&&r.source.start_ticks==123&&r.source.registration_id==11);
    assert(r.destination.host_id==Key(11)&&r.destination.boot_id==Key(43)&&r.destination.pid==101&&r.destination.start_ticks==124&&r.destination.registration_id==12);
    assert(r.source_session==13&&r.destination_session==14&&r.route_id==15&&x.routes[1].route_id==16&&x.routes[1].source.registration_id==17);
    assert(r.error==-EIO&&r.reason==static_cast<unsigned>(n::CompatibilityReason::TYPE_MISMATCH)&&r.state==static_cast<unsigned>(n::TopicRouteState::ACTIVE)&&r.proxy_recovery==static_cast<unsigned>(n::ProxyRecoveryState::RECOVERED));
    assert(r.packets_sent==21&&r.packets_received==22&&r.samples_published==23&&r.dropped==24&&r.duplicate==25&&r.out_of_order==26&&r.missed_sequence==27&&r.malformed==28&&r.local_samples_skipped==29);
    Wait([&]{return Value(tool,12).status==DiagnosticsState::UNSUPPORTED;});
    n::PeerDiscovery incompatible;auto ic=Config(13,port);ic.profile.network.major=2;assert(!incompatible.Start(ic));
    Wait([&]{return Value(tool,13).status==DiagnosticsState::INCOMPATIBLE;});
    const auto old_session=y.gateway_session;b.discovery.Stop();assert(!b.discovery.Start(b.config));
    Wait([&]{auto v=Value(tool,11);return v.status==DiagnosticsState::VALID&&v.gateway_session!=old_session;});
    a.mode=2;Wait([&]{auto v=Value(tool,10);return v.status==DiagnosticsState::TRUNCATED&&v.truncated&&v.total_routes==100&&v.routes.size()<100&&!v.routes.empty();});
    a.mode=1;Wait([&]{auto v=Value(tool,10);return v.status==DiagnosticsState::TIMEOUT&&v.error==-ETIMEDOUT&&v.routes.empty()&&!v.observed_at_unix_ms;});
    auto before=b.requests.load();Wait([&]{return b.requests>before+1;}); // silent Host cannot starve the other Gateway
    a.mode=3;Wait([&]{auto v=Value(tool,10);return v.status==DiagnosticsState::ERROR&&v.error==-EPROTO&&v.routes.empty();});
    a.mode=0;Wait([&]{return Value(tool,10).status==DiagnosticsState::VALID;});
    assert(Value(tool,10).routes[0].rates.status==NetworkRateState::MEASURING);
    std::vector<std::unique_ptr<Probe>> many;
    for(unsigned h=20;h<32;++h)many.push_back(std::make_unique<Probe>(h,port));
    Wait([&]{for(unsigned h=20;h<32;++h)if(Value(tool,h).status!=DiagnosticsState::VALID)return false;return true;});
    const auto request_count=[&]{int total=a.requests+b.requests;for(const auto& p:many)total+=p->requests;return total;};
    const int initial=request_count();const auto window=std::chrono::steady_clock::now();
    std::this_thread::sleep_for(1500ms);const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-window).count();
    assert(request_count()-initial<=int(elapsed*10)+2); // aggregate one admission per tick
    for(const auto& p:many)assert(p->requests>0);
    // Snapshots remain nonblocking while the worker is inside a bounded query.
    a.mode=1;before=a.requests;Wait([&]{return a.requests>before;});auto start=std::chrono::steady_clock::now();
    for(int i=0;i<100;++i)(void)tool.GetNetworkSnapshot();assert(std::chrono::steady_clock::now()-start<100ms);
    tool.StopNetwork();assert(std::chrono::steady_clock::now()-start<400ms);assert(tool.GetNetworkSnapshot().hosts.empty());
    unsupported.Stop();incompatible.Stop();std::cout<<"Network diagnostics backend: real counters, empty/restart/LOST, identities, nine counters, fair rotation, unsupported/timeout/error/truncated, nonblocking Snapshot and Stop PASS\n";
}
