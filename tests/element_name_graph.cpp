#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf_tool/ui/graph_model.hpp"
#include "kcf/network/control_node.hpp"
#include "kcf/ipc/publisher.hpp"
#include "parameter_demo_type.hpp"
#include <cassert>
#include "kcf/introspection/introspection_client.hpp"
#include <cerrno>
#include <functional>
#include <chrono>
#include <thread>
#include <set>
#include <iostream>
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
extern char** environ;
using namespace std::chrono_literals;
using namespace kcf_tool;
namespace n=kcf::network;
template<class F>void Until(F f,const std::function<void()>& dump={}){auto end=std::chrono::steady_clock::now()+10s;while(!f()){if(std::chrono::steady_clock::now()>=end){if(dump)dump();assert(false && "readiness deadline");}std::this_thread::sleep_for(40ms);}}
static void Dump(KcfBackend& tool,const std::vector<ElementInfo>& elements,const GraphModel& graph,pid_t fixture,int refresh){
    std::cerr<<"[graph-diag mono_ns="<<std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()<<"] refresh="<<refresh<<" fixture="<<fixture<<" alive="<<(kill(fixture,0)==0)<<" apps="<<tool.GetApplications().size()<<" elements="<<elements.size()<<" nodes="<<graph.nodes.size()<<" edges="<<graph.edges.size()<<'\n';
    for(const auto& a:tool.GetApplications())std::cerr<<"application "<<a.name<<" supervisor="<<a.supervisor.pid<<':'<<a.supervisor.process_start_ticks<<" state="<<a.state<<'\n';
    kcf::IntrospectionClient client;
    for(const auto& e:elements){
        std::cerr<<"element "<<e.name<<" runtime="<<e.identity.pid<<':'<<e.identity.process_start_ticks<<" supervisor="<<e.supervisor.pid<<':'<<e.supervisor.process_start_ticks<<" state="<<e.state<<'\n';
        kcf::RuntimeInfo runtime;runtime.pid=e.identity.pid;runtime.process_start_ticks=e.identity.process_start_ticks;std::vector<kcf::EndpointInfo> eps;const int error=client.ListEndpoints(runtime,eps);
        std::cerr<<"ListEndpoints error="<<error<<" count="<<eps.size()<<'\n';
        for(const auto& ep:eps)std::cerr<<"endpoint "<<ep.name<<" kind="<<int(ep.kind)<<" role="<<int(ep.role)<<" registration="<<ep.registration_id<<" type="<<ep.type_id<<'\n';
    }
    for(const auto& t:tool.GetTopics())std::cerr<<"backend-topic "<<t.name<<" role="<<t.role<<" runtime="<<t.identity.runtime.pid<<':'<<t.identity.runtime.process_start_ticks<<" registration="<<t.identity.registration_id<<'\n';
    for(const auto& n:graph.nodes)std::cerr<<"node "<<n.id<<" label="<<n.label<<'\n';
    for(const auto& e:graph.edges)std::cerr<<"edge "<<e.from<<" -> "<<e.to<<" role="<<e.role<<" registration="<<e.endpoint.registration_id<<'\n';
}
int main(int argc,char** argv){assert(argc==3);alarm(60);
    const std::string topic="/graph_name_test_"+std::to_string(getpid());kcf::Publisher<Value> publisher;assert(!publisher.Create(topic));
    KcfBackend tool;assert(!tool.StartNetwork({"127.0.0.1",49651}));
    std::set<std::string> prior_local,prior_remote;std::uint64_t prior_session=0;
    for(int run=0;run<2;++run){
        std::vector<std::string> args{argv[1],"--application-name","NameProbe"};
        const std::set<std::string> names{"Motor","IMU","EKF"};
        for(const auto& name:names){if(args.size()>3)args.push_back("--next");args.insert(args.end(),{argv[2],"--child",topic,"sentinel","--element-name",name,"--health-timeout-ms","2000"});}
        std::vector<char*> av;for(auto& a:args)av.push_back(a.data());av.push_back(nullptr);pid_t pid;assert(!posix_spawn(&pid,av[0],nullptr,nullptr,av.data(),environ));
        std::vector<ElementInfo> elements;int refresh=0;
        // Runtime names become visible before Setup registers the Subscriber. Wait for
        // the fixture contract, not Graph edge count, using one Backend snapshot.
        Until([&]{
            refresh=tool.Refresh();assert(!refresh);elements=tool.GetElements();
            bool running=false;
            for(const auto& a:tool.GetApplications())if(a.supervisor.pid==pid)running=a.state=="RUNNING";
            std::set<std::string> seen;const auto topics=tool.GetTopics();
            for(const auto& e:elements)if(e.supervisor.pid==pid){
                assert(e.application_name=="NameProbe");seen.insert(e.name);
                if(e.state!="RUNNING")return false;
                unsigned matches=0;
                for(const auto& t:topics)if(t.name==topic&&t.role=="SUBSCRIBER"&&t.identity.runtime==e.identity&&t.identity.registration_id)++matches;
                if(matches!=1)return false;
            }
            return running&&seen==names;
        },[&]{Dump(tool,elements,BuildGraph(tool.GetApplications(),elements,tool.GetTopics(),{},{}),pid,refresh);});
        auto graph=BuildGraph(tool.GetApplications(),elements,tool.GetTopics(),{},{});
        std::set<std::string> local_ids,labels;
        for(const auto& node:graph.nodes)if(node.kind==GraphKind::Element&&node.group.find(std::to_string(pid)+":")==0){labels.insert(node.label);local_ids.insert(node.id);}
        assert(labels==names&&local_ids.size()==3);
        std::cerr<<"[graph-ready] generation="<<run<<" fixture="<<pid<<" runtime/endpoint identities ready\n";
        unsigned edges=0;for(const auto& edge:graph.edges)if(local_ids.count(edge.to)||local_ids.count(edge.from))++edges;if(edges!=3)Dump(tool,elements,graph,pid,refresh);assert(edges==3); // same Topic, three independent Runtime identities
        for(const auto& id:prior_local)assert(!local_ids.count(id));prior_local=local_ids;
        n::ControlConfig cfg;cfg.discovery.host.bytes[0]=42;cfg.discovery.boot_id.bytes[0]=24;cfg.discovery.interface_address="127.0.0.1";cfg.discovery.port=49651;cfg.discovery.announce_interval=60ms;cfg.metadata_interval=50ms;
        for(const auto& e:elements)if(e.supervisor.pid==pid)cfg.metadata.runtime_pids.push_back(e.identity.pid);
        n::ControlNode gateway;assert(!gateway.Start(cfg));NetworkSnapshot snapshot;
        Until([&]{snapshot=tool.GetNetworkSnapshot();for(const auto& h:snapshot.hosts)if(h.metadata_ready&&h.endpoints.size()==3&&h.session!=prior_session){std::set<std::string> seen;for(const auto& e:h.endpoints){if(e.application!="NameProbe")return false;seen.insert(e.element);}if(seen==names)return true;}return false;});
        auto remote=AddRemoteGraph({},snapshot);std::set<std::string> remote_ids;labels.clear();
        for(const auto& node:remote.nodes)if(node.kind==GraphKind::Element){labels.insert(node.label);remote_ids.insert(node.id);assert(node.detail.find("Application: NameProbe")!=std::string::npos);}
        assert(labels==names&&remote_ids.size()==3);for(const auto& id:prior_remote)assert(!remote_ids.count(id));prior_remote=remote_ids;
        for(const auto& h:snapshot.hosts)if(h.metadata_ready&&h.endpoints.size()==3){prior_session=h.session;std::set<std::string> endpoint_ids;for(const auto& e:h.endpoints){assert(e.name==topic);endpoint_ids.insert(e.id);}assert(endpoint_ids.size()==3);}
        gateway.Stop();kill(pid,SIGTERM);int status=0;assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    }
    tool.StopNetwork();publisher.Close();publisher.Unlink();std::cout<<"Real Bringup -> Local Elements/Graph -> Network Metadata -> Remote Graph names, same-topic identity and restart/session refresh PASS\n";
}
