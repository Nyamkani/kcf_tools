// Delayed manual-style reconnect; unrelated live runtimes may coexist.
#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf/introspection/introspection_client.hpp"
#include "kcf/dynamic/dynamic_topic_reader.hpp"
#include <cassert>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std::chrono_literals;
template<class F> void Until(F f) {
    const auto deadline=std::chrono::steady_clock::now()+10s;
    while(!f()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::sleep_for(20ms);}
}
struct Fixture {
    pid_t pid;
    Fixture(const char* exe,const std::string& prefix,const char* option) {
        const auto parent=getpid();pid=fork();assert(pid>=0);
        if(!pid){assert(prctl(PR_SET_PDEATHSIG,SIGTERM)==0);if(getppid()!=parent)_exit(127);
            execl(exe,exe,"--element",prefix.c_str(),option,nullptr);_exit(127);}
    }
    ~Fixture(){kill(pid,SIGTERM);int status=0;assert(waitpid(pid,&status,0)==pid);
        assert(WIFEXITED(status)&&WEXITSTATUS(status)==0);}
};
std::string Value(const kcf_tool::DataSnapshot& data,const char* name) {
    for(const auto& field:data.fields)if(field.name==name)return field.value;
    assert(false);return {};
}
int main(int argc,char** argv) {
    assert(argc==2||argc==3);alarm(30);
    const auto prefix="/tool_reconnect_"+std::to_string(getpid());
    Fixture fixture(argv[1],prefix,argc==3?"--depth4":"");
    kcf_tool::KcfBackend backend;kcf_tool::TopicInfo topic;
    Until([&]{assert(backend.Refresh()==0);return backend.GetTopicInfo(prefix+"/topic",topic)==0;});
    const auto original=topic.identity;
    kcf::IntrospectionClient discovery;kcf::RuntimeInfo runtime;
    std::vector<kcf::RuntimeInfo> runtimes;assert(discovery.ListRuntimes(runtimes)==0);
    for(const auto& r:runtimes)if(r.pid==fixture.pid)runtime=r;
    const auto check_open=[&]{
        kcf::TypeDescriptor descriptor;
        const int type_result=discovery.GetType(runtime,topic.type_id,descriptor);
        std::cout<<"GetType="<<type_result<<std::endl;assert(type_result==0);
        kcf::DynamicTopicReader reader;
        const int open_result=reader.Open(topic.name,descriptor);
        std::cout<<"DynamicTopicReader::Open="<<open_result<<std::endl;assert(open_result==0);
        const int echo_result=backend.StartTopicEcho(topic.identity);
        std::cout<<"StartTopicEcho="<<echo_result<<std::endl;assert(echo_result==0);
    };
    check_open();kcf_tool::DataSnapshot data;
    Until([&]{const int r=backend.ReadTopicEcho(data);assert(r==0||r==-EAGAIN);return r==0;});
    assert(Value(data,"x")=="1.25");
    assert(kill(fixture.pid,SIGUSR1)==0);
    Until([&]{assert(backend.ReadTopicEcho(data)==0);return Value(data,"x")=="9.5";});
    for(int i=0;i<3;++i){
        // Unlike the fast smoke test, leave a mapped reader alive before and
        // after recreation, then Refresh well after the new SHM is published.
        std::this_thread::sleep_for(1s);
        assert(kill(fixture.pid,SIGWINCH)==0);
        std::this_thread::sleep_for(1s);
        assert(backend.ReadTopicEcho(data)==-ESTALE);
        const auto previous=topic.identity;
        assert(backend.Refresh()==0 && backend.GetTopicInfo(prefix+"/topic",topic)==0);
        assert(topic.identity.runtime==original.runtime);
        assert(topic.identity.registration_id!=previous.registration_id);
        check_open();assert(backend.ReadTopicEcho(data)==0);
        assert(Value(data,"x")=="9.5" && Value(data,"sequence")=="8");
    }
    backend.StopTopicEcho();std::cout<<"PASS delayed reconnect, depth="<<(argc==3?4:1)<<std::endl;
}
