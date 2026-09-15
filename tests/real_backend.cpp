#ifdef NDEBUG
#undef NDEBUG
#endif
// Backend consumer has no KCF includes or application payload definitions.
#include "kcf_tool/backend/kcf_backend.hpp"
#include <cassert>
#include <cerrno>
#include <chrono>
#include <iostream>
#include <thread>
#include <signal.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <unistd.h>
using namespace kcf_tool;
using namespace std::chrono_literals;
template<class F> void Until(F f){auto deadline=std::chrono::steady_clock::now()+10s;while(!f()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::sleep_for(20ms);}}
struct Process {pid_t pid;Process(const char* exe,const char* mode,const std::string& prefix,const std::string& remote=""){const auto parent=getpid();pid=fork();assert(pid>=0);if(!pid){assert(prctl(PR_SET_PDEATHSIG,SIGTERM)==0);if(getppid()!=parent)_exit(127);execl(exe,exe,mode,prefix.c_str(),remote.c_str(),nullptr);_exit(127);}}
    void Stop(){if(pid<=0)return;assert(kill(pid,SIGTERM)==0);int status;assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);pid=-1;}
    ~Process(){if(pid>0){kill(pid,SIGTERM);waitpid(pid,nullptr,0);}}};
FieldValue& Field(DataSnapshot& s,const std::string& name){for(auto& f:s.fields)if(f.name==name)return f;assert(false);return s.fields[0];}
int main(int argc,char** argv){assert(argc==2);alarm(90);KcfBackend backend;assert(backend.Refresh()==0);assert(backend.GetConnectionState()==BackendConnectionState::DISCONNECTED);assert(backend.GetElements().empty()&&backend.GetApplications().empty());
    DataSnapshot unchanged{"sentinel",42,1,{}};assert(backend.ReadTopicLatest("missing",unchanged)==-ENOENT&&unchanged.type_name=="sentinel");
    bool called=false;assert(backend.StartTopicMonitor("topic",[&](const auto&){called=true;})==-ENOTSUP&&!called);
    const auto base="/kt3_"+std::to_string(getpid());Process standalone(argv[1],"--element",base+"_standalone");
    Until([&]{assert(backend.Refresh()==0);return !backend.QueryServices().empty() && backend.GetTopics().size()==3 && backend.GetParameters().size()==2;});assert(backend.GetConnectionState()==BackendConnectionState::CONNECTED);
    auto elements=backend.GetElements();assert(elements.size()==1&&elements[0].mode=="STANDALONE"&&elements[0].application_name=="Standalone");
    TopicInfo topic;assert(backend.GetTopicInfo(base+"_standalone/topic",topic)==0&&topic.role=="PUBLISHER"&&topic.type_id);
    auto old_identity=topic.identity;DataSnapshot sample;Until([&]{int r=backend.ReadTopicLatest(old_identity,sample);assert(r==0||r==-EAGAIN);return r==0;});
    assert(Field(sample,"sequence").value=="7"&&Field(sample,"x").value=="1.25"&&Field(sample,"count").value=="-3");assert((Field(sample,"values").array_values==std::vector<std::string>{"2","4","6","8"}));
    assert(backend.ReadTopicLatest(base+"_standalone/legacy",unchanged)==-ENOTSUP&&unchanged.type_name=="sentinel");
    // A persistent instance must detect recreation between reads, even though
    // PID/start ticks/name/type/layout are unchanged. A fresh one-shot sees B.
    assert(backend.StartTopicEcho(old_identity)==0);
    DataSnapshot echo;
    for(int i=0;i<20;++i){assert(backend.ReadTopicEcho(echo)==0);assert(Field(echo,"x").value=="1.25");}
    assert(kill(standalone.pid,SIGWINCH)==0);
    Until([&]{const int r=backend.ReadTopicEcho(echo);assert(r==0||r==-ESTALE);
        assert(Field(echo,"x").value=="1.25");return r==-ESTALE;});
    assert(backend.ReadTopicEcho(echo)==-EBADF);
    assert(backend.Refresh()==0);
    assert(backend.GetTopicInfo(base+"_standalone/topic",topic)==0);
    assert(topic.identity.runtime==old_identity.runtime && topic.type_id==sample.type_id);
    assert(topic.identity.registration_id!=old_identity.registration_id);
    old_identity=topic.identity;
    assert(backend.StartTopicEcho(old_identity)==0);
    Until([&]{int r=backend.ReadTopicEcho(echo);assert(r==0||r==-EAGAIN);return r==0;});
    assert(Field(echo,"x").value=="9.5");
    DataSnapshot once;assert(backend.ReadTopicLatest(old_identity,once)==0 && Field(once,"x").value=="9.5");
    assert(backend.ReadTopicEcho(echo)==0); // one-shot did not close/replace echo
    backend.StopTopicEcho();backend.StopTopicEcho();assert(backend.ReadTopicEcho(echo)==-EBADF);
    assert(backend.StartTopicEcho(old_identity)==0);
    assert(backend.StartTopicEcho(EndpointIdentity{})==-ENOENT && backend.ReadTopicEcho(echo)==-EBADF);
    Process supervised(argv[1],"--supervisor",base+"_supervised",base+"_standalone");
    Until([&]{assert(backend.Refresh()==0);auto applications=backend.GetApplications();return backend.GetElements().size()==2&&backend.QueryServices().size()==2&&applications.size()==1&&applications[0].state=="RUNNING";});
    auto apps=backend.GetApplications();assert(apps.size()==1&&apps[0].name=="tool_test_application"&&apps[0].supervisor.pid==supervised.pid&&apps[0].managed_element_count==1&&apps[0].revision>0);
    bool grouped=false;for(const auto& e:backend.GetElements())if(e.mode=="SUPERVISED"){assert(e.name=="friendly_element"&&e.supervisor==apps[0].supervisor);grouped=true;}assert(grouped);
    unsigned copies=0;for(const auto& t:backend.GetTopics())if(t.name==base+"_standalone/topic")++copies;assert(copies==3); // publisher, local subscriber, remote subscriber
    assert(backend.GetTopicInfo(base+"_standalone/topic",topic)==0&&topic.identity==old_identity);
    TopicInfo other_topic;assert(backend.GetTopicInfo(base+"_supervised/topic",other_topic)==0);
    assert(backend.StartTopicEcho(old_identity)==0 && backend.ReadTopicEcho(echo)==0 && Field(echo,"x").value=="9.5");
    assert(backend.StartTopicEcho(other_topic.identity)==0);
    Until([&]{int r=backend.ReadTopicEcho(echo);assert(r==0||r==-EAGAIN);return r==0;});
    assert(Field(echo,"x").value=="1.25");backend.StopTopicEcho();
    ParameterInfo parameter;assert(backend.GetParameter(base+"_standalone/config",parameter)==0&&parameter.role=="OWNER"&&parameter.writable&&!parameter.text_editable);
    assert(Field(parameter.snapshot,"gain").value=="5"&&Field(parameter.snapshot,"enabled").value=="true");
    auto settings=parameter.snapshot;Field(settings,"gain").value="23";Field(settings,"enabled").value="false";assert(backend.SetParameter(parameter.identity,settings)==0);
    DataSnapshot read;assert(backend.GetParameter(parameter.identity,read)==0&&Field(read,"gain").value=="23");
    auto bad=settings;bad.fields[0].name="unknown";assert(backend.SetParameter(parameter.identity,bad)==-EINVAL);
    bad=settings;bad.type_id++;assert(backend.SetParameter(parameter.identity,bad)==-EPROTOTYPE);
    bad=settings;Field(bad,"gain").value="2147483648";assert(backend.SetParameter(parameter.identity,bad)==-ERANGE);
    bad=settings;Field(bad,"gain").kind=ValueKind::STRING;assert(backend.SetParameter(parameter.identity,bad)==-EPROTOTYPE);
    assert(backend.SetParameter(parameter.name,"9")==-ENOTSUP);
    assert(backend.GetParameter(parameter.identity,read)==0&&Field(read,"gain").value=="23");
    ServiceInfo service;for(const auto& s:backend.QueryServices())if(s.name==base+"_standalone/add")service=s;assert(service.port&&service.request_type_id&&service.response_type_id);
    DataSnapshot request;assert(backend.GetTypeTemplate(service.identity.runtime,service.request_type_id,request)==0);Field(request,"a").value="4";Field(request,"b").value="6";
    DataSnapshot response;Until([&]{assert(backend.CallService(service.identity,request,response)==0);return Field(response,"watches").value!="0";});
    assert(Field(response,"sum").value=="10"&&Field(response,"gain").value=="23");auto count=std::stoul(Field(response,"calls").value);
    bad=request;bad.fields[0].name="wrong";assert(backend.CallService(service.identity,bad,unchanged)==-EINVAL&&unchanged.type_name=="sentinel");
    bad=request;bad.fields[0].type_name="float32";assert(backend.CallService(service.identity,bad,unchanged)==-EPROTOTYPE);
    assert(backend.CallService(service.identity,request,response)==0&&std::stoul(Field(response,"calls").value)==count+1);
    supervised.Stop();standalone.Stop();assert(backend.ReadTopicLatest(old_identity,unchanged)<0&&unchanged.type_name=="sentinel");
    assert(backend.CallService(service.identity,request,unchanged)<0&&unchanged.type_name=="sentinel");
    assert(backend.Refresh()==0&&backend.GetElements().empty()&&backend.QueryServices().empty()&&backend.GetConnectionState()==BackendConnectionState::DISCONNECTED);
    std::cout<<"PASS real backend: no runtime, standalone/supervised membership, duplicate endpoints, latest arrays, Parameter Get/Set/watch, Service type safety and stale processes\n";
}
