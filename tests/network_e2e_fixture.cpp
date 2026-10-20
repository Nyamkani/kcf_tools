// Reuse Framework's actual Publisher, Subscriber and control-channel fixture.
#define main FrameworkVirtualMain
#include "host_fixture.cpp"
#undef main
int main(int argc,char** argv){
    try {
        if(argc!=3)return 2;
        const int host=std::stoi(argv[2]);
#ifdef KCF_TOOL_TEST_REMOTE_CONTROL
        if(std::string(argv[1])=="shared"){
            Begin();kcf::Publisher<Value> publisher;Check(publisher.Create("/virtual/shared/topic",8));std::atomic<bool> active{true};
            std::thread worker([&]{int count=host*100000;while(active){Check(publisher.Publish({++count,false,1.5}));std::this_thread::sleep_for(5ms);}});
            Emit({{"ready",1}});std::string line;while(std::getline(std::cin,line)&&line!="QUIT")Emit({{"ready",1}});
            active=false;worker.join();publisher.Close();publisher.Unlink();End();return 0;
        }
#endif
        if(std::string(argv[1])!="gateway")return FrameworkVirtualMain(argc,argv);
        n::ControlConfig c;c.discovery.host=Id(host);c.discovery.boot_id=Id(80+host);
        c.discovery.interface_address=host==1?"10.203.0.2":"10.203.0.3";
        c.discovery.announce_interval=100ms;c.discovery.peer_timeout=1200ms;c.metadata_interval=50ms;
        c.topics.enabled=true;c.topics.max_hz=50;c.diagnostics_enabled=true;
        c.metadata.remote_topics={"/virtual/a/topic"};c.topics.local_types={{"/virtual/a/topic",Descriptor()}};
#ifdef KCF_TOOL_TEST_REMOTE_CONTROL
        c.parameter_get_clients=c.parameter_set_clients=c.service_call_clients=c.action_goal_clients=c.action_cancel_clients=c.descriptor_query_clients={Id(99)};
        c.metadata.remote_parameters={Prefix(host)+"/parameter"};c.metadata.remote_services={Prefix(host)+"/service"};c.metadata.remote_actions={Prefix(host)+"/action"};
        c.metadata.remote_topics={"/virtual/a/topic","/virtual/b/topic","/virtual/shared/topic"};
#endif
        n::ControlNode node;Check(node.Start(c));Emit({{"ready",1},{"pid",getpid()}});
        std::string command;
        while(std::getline(std::cin,command)&&command!="QUIT"){
            if(command=="STOP")node.Stop();
            if(command=="START"||command=="DISABLE"){
                node.Stop();c.diagnostics_enabled=command=="START";Check(node.Start(c));
            }
            auto d=node.GetTopicDiagnostics();Emit({{"ready",1},{"proxies",static_cast<long long>(d.proxies.size())},{"send_routes",d.active_send_routes},{"receive_routes",d.active_receive_routes}});
        }
        node.Stop();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}
