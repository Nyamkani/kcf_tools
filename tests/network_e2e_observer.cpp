#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf_tool/ui/graph_widget.hpp"
#include <QApplication>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>
#include <unistd.h>
using namespace kcf_tool;
std::string Key(const RouteEndpointIdentity& e){return e.host_id+":"+e.boot_id+":"+std::to_string(e.pid)+":"+std::to_string(e.start_ticks)+":"+std::to_string(e.registration_id);}
std::size_t Edges(const GraphModel& m){std::size_t n=0;for(const auto& e:m.edges)n+=e.network_route;return n;}
int main(int argc,char** argv){
    QApplication app(argc,argv);KcfBackend backend;assert(!backend.StartNetwork({"10.203.0.2",37651}));
    GraphWidget graph;graph.show();std::cout<<"{\"ready\":1,\"pid\":"<<getpid()<<"}"<<std::endl;
    std::string command;
    while(std::getline(std::cin,command)&&command!="QUIT"){
        auto s=backend.GetNetworkSnapshot();graph.SetSnapshot({}, {}, {}, {}, {}, s);app.processEvents();
        if(command=="FRESHNESS"){
            assert(Edges(graph.Model())==1);
            std::this_thread::sleep_for(std::chrono::seconds(11));app.processEvents();
            assert(Edges(graph.Model())==1); // No timer: displayed evidence remains until refresh.
            graph.SetSnapshot({}, {}, {}, {}, {}, s);app.processEvents();assert(Edges(graph.Model())==0);
            std::cout<<"{\"freshness\":1,\"elapsed_seconds\":11,\"edges\":0}"<<std::endl;continue;
        }
        const RemoteRoute *tx=nullptr,*rx=nullptr;int online=0,unavailable=0,timeouts=0,unsupported=0;std::uint64_t session=0;
        for(const auto& h:s.hosts){online+=h.state=="ONLINE";timeouts+=h.diagnostics.status==DiagnosticsState::TIMEOUT;unsupported+=h.diagnostics.status==DiagnosticsState::UNSUPPORTED;unavailable+=h.diagnostics.status!=DiagnosticsState::VALID;
            for(const auto& r:h.diagnostics.routes)if(r.state==1){if(r.sending)tx=&r;else{rx=&r;session=h.session;}}
        }
        const auto edges=Edges(graph.Model());assert(edges<=1);
        if(edges){assert(tx&&rx);assert(Key(tx->source)==Key(rx->source)&&Key(tx->destination)==Key(rx->destination));
            assert(tx->source_session==rx->source_session&&tx->destination_session==rx->destination_session&&tx->route_id==rx->route_id);
            for(const auto& e:graph.Model().edges)if(e.network_route){assert(e.from=="remote-endpoint:"+Key(tx->source)&&e.to=="remote-endpoint:"+Key(tx->destination));
                assert(e.detail.find("route_id="+std::to_string(tx->route_id))!=std::string::npos);
                assert(e.detail.find("Network Send Hz:")!=std::string::npos&&e.detail.find("Network Receive Hz:")!=std::string::npos);
            }
            int paths=0;for(auto* item:graph.findChild<QGraphicsView*>("graphView")->scene()->items())if(auto* path=dynamic_cast<QGraphicsPathItem*>(item)){
                if(path->toolTip().contains("Verified ACTIVE")){++paths;assert(path->pen().color()==QColor("#007f9e")&&path->pen().style()==Qt::SolidLine);}
                else assert(path->pen().style()==Qt::DashLine);
            }assert(paths==1);
            auto stale=s;for(auto& h:stale.hosts)h.diagnostics.observed_at_unix_ms-=11000;
            assert(Edges(AddRemoteGraph({},stale))==0); // Refresh rejects evidence older than 10 seconds.
            assert(Edges(graph.Model())==1); // Saved scene remains until refreshed.
        }
        const bool measured=tx&&rx&&tx->rates.status==NetworkRateState::VALID&&rx->rates.status==NetworkRateState::VALID;
        std::cout<<"{\"online\":"<<online<<",\"edges\":"<<edges<<",\"active\":"<<bool(tx)+bool(rx)<<",\"unavailable\":"<<unavailable
            <<",\"timeouts\":"<<timeouts<<",\"unsupported\":"<<unsupported<<",\"session\":"<<session<<",\"measured\":"<<measured<<",\"send\":"<<(tx?tx->rates.send_hz:0)
            <<",\"receive\":"<<(rx?rx->rates.receive_hz:0)<<",\"proxy\":"<<(rx?rx->rates.proxy_publish_hz:0)
            <<",\"skipped\":"<<(tx?tx->rates.local_skipped_per_second:0)<<",\"sent_count\":"<<(tx?tx->packets_sent:0)
            <<",\"receive_count\":"<<(rx?rx->packets_received:0)<<",\"proxy_count\":"<<(rx?rx->samples_published:0)<<"}"<<std::endl;
    }
    backend.StopNetwork();return 0;
}
