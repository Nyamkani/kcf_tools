// Application fixture only: external KCF public API; no copied core source.
#include "kcf/introspection/type_descriptor.hpp"
#include "kcf/process/process_runtime.hpp"
#include "kcf/ipc/publisher.hpp"
#include "kcf/ipc/subscriber.hpp"
#include "kcf/parameter/parameter.hpp"
#include "kcf/service/service_server.hpp"
#include "bringup/bringup.hpp"
#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <signal.h>
#include <unistd.h>
volatile sig_atomic_t echo_changed = 0;
volatile sig_atomic_t echo_unlink = 0;
volatile sig_atomic_t echo_recreate = 0;
bool hold_sample=false,service_fixtures=false;
std::uint32_t topic_depth=1;
std::atomic<bool> reset_requested{false};
static_assert(std::atomic<bool>::is_always_lock_free);
void RequestFixtureReset(int){reset_requested.store(true,std::memory_order_relaxed);}
volatile sig_atomic_t parameter_recreate=0,parameter_update=0;
void RecreateParameter(int){parameter_recreate=1;}
void UpdateParameter(int){parameter_update=1;}
void RecreateEcho(int) { echo_recreate = 1; }
void UnlinkEcho(int) { echo_unlink = 1; }
void ChangeEcho(int) { echo_changed = 1; }
struct Sample {std::uint64_t sequence;float x;std::int32_t count;float values[4];};
struct Config {std::int32_t gain;bool enabled;std::uint8_t byte{7};std::int8_t small{-1};float ratio{1.5f};float values[3]{2,4,6};};
struct Request {std::int32_t a,b;std::uint8_t byte{0};std::int8_t small{0};float ratio{0};float values[3]{};};
struct Response {std::int32_t sum;std::uint32_t calls,watches;std::int32_t gain;std::int32_t old_gain{-1};bool success{true};float values[3]{};};
struct Legacy {int value;};
namespace kcf {
template<> struct TypeDescriptorTraits<Sample>{static constexpr bool defined=true;static TypeDescriptor Get() noexcept {
    auto d=MakeTypeDescriptor<Sample>("tool.test.Sample.v1");d.field_count=4;
    d.fields[0]=MakeField<Sample,std::uint64_t>("sequence",offsetof(Sample,sequence));d.fields[1]=MakeField<Sample,float>("x",offsetof(Sample,x));
    d.fields[2]=MakeField<Sample,std::int32_t>("count",offsetof(Sample,count));d.fields[3]=MakeField<Sample,float[4]>("values",offsetof(Sample,values));return d;}};
template<> struct TypeDescriptorTraits<Config>{static constexpr bool defined=true;static TypeDescriptor Get() noexcept {
    auto d=MakeTypeDescriptor<Config>("tool.test.Config.v1");d.field_count=6;d.fields[0]=MakeField<Config,std::int32_t>("gain",offsetof(Config,gain));d.fields[1]=MakeField<Config,bool>("enabled",offsetof(Config,enabled));
    d.fields[2]=MakeField<Config,std::uint8_t>("byte",offsetof(Config,byte));
    d.fields[3]=MakeField<Config,std::int8_t>("small",offsetof(Config,small));
    d.fields[4]=MakeField<Config,float>("ratio",offsetof(Config,ratio));
    d.fields[5]=MakeField<Config,float[3]>("values",offsetof(Config,values));return d;}};
template<> struct TypeDescriptorTraits<Request>{static constexpr bool defined=true;static TypeDescriptor Get() noexcept {
    auto d=MakeTypeDescriptor<Request>("tool.test.Request.v1");d.field_count=6;d.fields[0]=MakeField<Request,std::int32_t>("a",offsetof(Request,a));d.fields[1]=MakeField<Request,std::int32_t>("b",offsetof(Request,b));
    d.fields[2]=MakeField<Request,std::uint8_t>("byte",offsetof(Request,byte));d.fields[3]=MakeField<Request,std::int8_t>("small",offsetof(Request,small));
    d.fields[4]=MakeField<Request,float>("ratio",offsetof(Request,ratio));d.fields[5]=MakeField<Request,float[3]>("values",offsetof(Request,values));return d;}};
template<> struct TypeDescriptorTraits<Response>{static constexpr bool defined=true;static TypeDescriptor Get() noexcept {
    auto d=MakeTypeDescriptor<Response>("tool.test.Response.v1");d.field_count=7;d.fields[0]=MakeField<Response,std::int32_t>("sum",offsetof(Response,sum));d.fields[1]=MakeField<Response,std::uint32_t>("calls",offsetof(Response,calls));d.fields[2]=MakeField<Response,std::uint32_t>("watches",offsetof(Response,watches));d.fields[3]=MakeField<Response,std::int32_t>("gain",offsetof(Response,gain));d.fields[4]=MakeField<Response,std::int32_t>("old_gain",offsetof(Response,old_gain));d.fields[5]=MakeField<Response,bool>("success",offsetof(Response,success));d.fields[6]=MakeField<Response,float[3]>("values",offsetof(Response,values));return d;}};
}
std::uint16_t FreePort(){int fd=socket(AF_INET,SOCK_DGRAM,0);assert(fd>=0);sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);assert(bind(fd,reinterpret_cast<sockaddr*>(&a),sizeof(a))==0);socklen_t n=sizeof(a);assert(getsockname(fd,reinterpret_cast<sockaddr*>(&a),&n)==0);auto port=ntohs(a.sin_port);close(fd);return port;}
struct Element:kcf::ProcessElement {
    std::string prefix,remote;kcf::Publisher<Sample> pub;kcf::Subscriber<Sample> sub,remote_sub;
    kcf::Parameter<Config> owner,client,remote_client,previous;kcf::Publisher<Legacy> legacy;
    std::mutex config_access;kcf::ServiceServer service;std::atomic<unsigned> watches{0};unsigned calls=0;
    Element(std::string p,std::string r):prefix(std::move(p)),remote(std::move(r)){}
    int Setup() override {
        int r=pub.Create(prefix+"/topic",topic_depth);if(r)return r;
        if(topic_depth==4){
            // Wrap the queue before discovery; latest must skip retained older values.
            for(std::uint64_t i=0;i<8;++i){r=pub.Publish({100+i,-2.0f,99,{1,1,1,1}});if(r)return r;}
            r=pub.Publish({7,1.25f,-3,{2,4,6,8}});if(r)return r;
        }
        r=sub.Create(prefix+"/topic",[](const Sample&){});if(r)return r;
        r=owner.Create(prefix+"/config",{5,true},[&](const Config&){++watches;});if(r)return r;
        r=client.Open(prefix+"/config");if(r)return r;r=legacy.Create(prefix+"/legacy");if(r)return r;
        if(!remote.empty()){r=remote_sub.Create(remote+"/topic",[](const Sample&){});if(r)return r;r=remote_client.Open(remote+"/config");if(r)return r;}
        for(int attempt=0;attempt<20;++attempt){r=service.Create(FreePort());if(r!=-EADDRINUSE)break;}if(r)return r;
        r=service.Register<Request,Response>(10,prefix+"/add",[&](const Request& q,Response& response){std::lock_guard<std::mutex> lock(config_access);Config value{};assert(client.Get(value)==0);Config old{};const auto old_gain=previous.Get(old)==0?old.gain:-1;response={q.a+q.b,++calls,watches.load(),value.gain,old_gain};response.success=q.a>=0;for(int i=0;i<3;++i)response.values[i]=q.values[i];});if(r)return r;
        if(service_fixtures){
            r=service.Register<Request,Response>(11,prefix+"/slow",[&](const Request&,Response& response){++calls;std::this_thread::sleep_for(std::chrono::milliseconds(1200));response.calls=calls;});if(r)return r;
            r=service.Register<Legacy,Legacy>(12,prefix+"/undefined",[](const Legacy& request,Legacy& response){response=request;});if(r)return r;
        }
        return service.Start();
    }
    int Loop() override {
        if(parameter_recreate){
            std::lock_guard<std::mutex> lock(config_access);assert(previous.Open(prefix+"/config")==0);assert(client.Close()==0);
            assert(owner.Close()==0 && owner.Unlink()==0 && owner.Create(prefix+"/config",{88,true},[&](const Config&){++watches;})==0);
            assert(client.Open(prefix+"/config")==0);parameter_recreate=0;
            std::cout<<"PARAMETER_RECREATED"<<std::endl;
        }
        if(parameter_update){Config value{};assert(client.Get(value)==0);value.gain=66;value.enabled=!value.enabled;value.values[1]+=10;
            assert(client.Set(value)==0);parameter_update=0;std::cout<<"PARAMETER_UPDATED"<<std::endl;}
        if(hold_sample && !echo_changed)return 0;
        if(echo_recreate){assert(pub.Close()==0 && pub.Unlink()==0 && pub.Create(prefix+"/topic",topic_depth)==0);echo_changed=1;echo_recreate=0;}if(echo_unlink){pub.Unlink();echo_unlink=0;}int r=pub.Publish(echo_changed ? Sample{8,9.5f,12,{10,20,30,40}} : Sample{7,1.25f,-3,{2,4,6,8}});if(r)return r;return legacy.Publish({9});}
    void Shutdown() override {service.Stop();previous.Close();remote_client.Close();remote_sub.Close();client.Close();owner.Close();owner.Unlink();sub.Close();pub.Close();pub.Unlink();legacy.Close();legacy.Unlink();}
};
int main(int argc,char** argv){signal(SIGUSR1,ChangeEcho);signal(SIGUSR2,UnlinkEcho);signal(SIGWINCH,RecreateEcho);signal(SIGTTIN,RecreateParameter);signal(SIGTTOU,UpdateParameter);assert(prctl(PR_SET_PDEATHSIG,SIGTERM)==0);if(argc<3)return 2;std::string mode=argv[1],prefix=argv[2],remote=argc>3?argv[3]:"";
    if(mode=="--application"){
        signal(SIGUSR1,RequestFixtureReset);
        std::vector<bringup::ElementSpec> specs;
        for(int i=3;i<argc;++i)specs.push_back({"element_"+std::to_string(i-3),argv[0],{"--element",argv[i]}});
        bringup::Bringup application;int r=application.Setup(prefix,std::move(specs));if(r)return 1;
        std::atomic<bool> done{false};
        // Signal handler only sets a flag; RequestReset runs on a normal thread.
        std::thread reset([&]{while(!done){if(reset_requested.exchange(false,std::memory_order_relaxed)){application.RequestReset();}
            std::this_thread::sleep_for(std::chrono::milliseconds(10));}});
        r=application.Run();done=true;reset.join();return r?1:0;
    }
    if(mode=="--supervisor"){bringup::Bringup app;int r=app.Setup("tool_test_application",{{"friendly_element",argv[0],{"--element",prefix,remote}}});return r?1:app.Run();}
    if(remote=="--depth4"){topic_depth=4;remote.clear();}
    if(remote=="--services"){service_fixtures=true;remote.clear();}
    if(remote=="--no-sample"){hold_sample=true;remote.clear();}
    Element e(prefix,remote);kcf::ProcessRuntime runtime;runtime.SetLoopFrequency(100);return runtime.Run(e)?1:0;
}
