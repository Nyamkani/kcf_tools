// Reuse Framework's actual Publisher, Subscriber and control-channel fixture.
#define main FrameworkVirtualMain
#include "host_fixture.cpp"
#undef main
int main(int argc,char** argv){
    try {
        if(argc!=3)return 2;
        const int host=std::stoi(argv[2]);
        if(std::string(argv[1])!="gateway")return FrameworkVirtualMain(argc,argv);
        n::ControlConfig c;c.discovery.host=Id(host);c.discovery.boot_id=Id(80+host);
        c.discovery.interface_address=host==1?"10.203.0.2":"10.203.0.3";
        c.discovery.announce_interval=100ms;c.discovery.peer_timeout=1200ms;c.metadata_interval=50ms;
        c.topics.enabled=true;c.topics.max_hz=50;c.diagnostics_enabled=true;
        c.metadata.remote_topics={"/virtual/a/topic"};c.topics.local_types={{"/virtual/a/topic",Descriptor()}};
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
