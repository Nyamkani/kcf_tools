#include "kcf_tool/ui/graph_widget.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <cassert>
#include <map>
#include <set>
#include <chrono>
#include <QGraphicsPathItem>
#include <QPushButton>
using namespace kcf_tool;
NetworkSnapshot Snapshot(){
    NetworkSnapshot s;
    for(int i=0;i<2;++i){RemoteHost h;h.id="host"+std::to_string(i);h.boot_id="boot";h.label="same-host";
        h.state=i?"LOST":"ONLINE";h.framework_version="5.1.0";h.network_version="1.0";
        h.compatibility=i?"NETWORK_MAJOR_MISMATCH":"COMPATIBLE";h.capabilities="DISCOVERY REMOTE_TOPIC TARGET_ENDPOINT";
        h.session=1;h.revision=2;h.metadata_ready=true;
        for(const auto kind:{"T","P","S","A"}){RemoteEndpoint e;e.id=h.id+":boot:100:123:"+kind;e.runtime_id=h.id+":boot:100:123";
            e.application_id=h.id+":boot:50:99";e.application="same-app";e.element="same-element";e.name="/same-name";
            e.kind=kind;e.role=e.kind=="T"?"PUBLISHER":e.kind=="P"?"OWNER":"SERVER";h.endpoints.push_back(e);}
        s.hosts.push_back(h);
    }return s;
}
NetworkSnapshot DiagnosticsSnapshot(){
    auto s=Snapshot();s.route_status="Diagnostics snapshot available";s.hosts.resize(1);auto& h=s.hosts[0];
    auto& e=h.endpoints[0];e.id="host0:boot:100:123:7";
    auto& d=h.diagnostics;d.status=DiagnosticsState::VALID;d.gateway_host=h.id;d.gateway_boot=h.boot_id;d.gateway_session=h.session;d.observed_at_unix_ms=123456;
    RemoteRoute r;r.source={h.id,h.boot_id,100,123,7};r.destination={"other","boot2",200,234,8};
    r.source_session=h.session;r.destination_session=2;r.route_id=15;r.name=e.name;r.state=1;r.sending=true;r.error=-5;r.reason=7;
    r.packets_sent=101;r.packets_received=102;r.samples_published=103;r.local_samples_skipped=104;
    r.dropped=105;r.duplicate=106;r.out_of_order=107;r.missed_sequence=108;r.malformed=109;
    r.rates.status=NetworkRateState::VALID;r.rates.send_hz=42.5;r.rates.local_skipped_per_second=150.25;
    r.rates.receive_hz=41.5;r.rates.proxy_publish_hz=40.25;r.rates.interval_seconds=1.25;
    d.routes={r};d.total_routes=1;return s;
}
std::string TopicDetail(const NetworkSnapshot& s){
    GraphOptions all{true,true,true,{},true};auto m=AddRemoteGraph({},s,all);
    for(const auto& n:m.nodes)if(n.id=="remote-endpoint:"+s.hosts[0].endpoints[0].id)return n.detail;
    assert(false);return {};
}
void DiagnosticsDetails(){
    const auto base=DiagnosticsSnapshot();auto text=TopicDetail(base);
    for(const auto* expected:{"Network Send Hz: 42.50","Local Skipped/s: 150.25","Measured interval (s): 1.25",
        "route_id=15","State: ACTIVE","Sending: true","Source: host0:boot:100:123:7 / session 1",
        "Destination: other:boot2:200:234:8 / session 2","Error: -5 / reason: 7","packets_sent=101",
        "packets_received=102","samples_published=103","local_samples_skipped=104","dropped=105",
        "duplicate=106","out_of_order=107","missed_sequence=108","malformed=109"})assert(text.find(expected)!=std::string::npos);
    assert(text.find("Network Receive Hz:")==std::string::npos);
    auto s=base;s.hosts[0].endpoints[0].role="SUBSCRIBER";auto& receive=s.hosts[0].diagnostics.routes[0];
    std::swap(receive.source,receive.destination);std::swap(receive.source_session,receive.destination_session);receive.sending=false;
    text=TopicDetail(s);assert(text.find("Network Receive Hz: 41.50")!=std::string::npos&&text.find("Proxy Publish Hz: 40.25")!=std::string::npos);
    assert(text.find("not application subscriber processing rate")!=std::string::npos&&text.find("Network Send Hz:")==std::string::npos);
    s=base;auto another=s.hosts[0].diagnostics.routes[0];another.route_id=16;another.destination.registration_id=88;another.rates.send_hz=3;
    s.hosts[0].diagnostics.routes.push_back(another);another.route_id=99;++another.source.registration_id;s.hosts[0].diagnostics.routes.push_back(another);
    s.hosts[0].diagnostics.total_routes=3;text=TopicDetail(s);
    assert(text.find("Route 2 (route_id=16)")!=std::string::npos&&text.find("Network Send Hz: 3.00")!=std::string::npos&&text.find("route_id=99")==std::string::npos);
    // Names are identical; every endpoint component and the local route session matter.
    for(int field=0;field<7;++field){s=base;auto& r=s.hosts[0].diagnostics.routes[0];
        if(field==0)r.source.host_id+="x";if(field==1)r.source.boot_id+="x";if(field==2)++r.source.pid;
        if(field==3)++r.source.start_ticks;if(field==4)++r.source.registration_id;if(field==5)++r.source_session;if(field==6)r.sending=false;
        text=TopicDetail(s);assert(text.find("No matching diagnostics route")!=std::string::npos&&text.find("packets_sent=101")==std::string::npos);
    }
    s=base;s.hosts[0].diagnostics.routes[0].rates.status=NetworkRateState::MEASURING;
    assert(TopicDetail(s).find("Network Send Hz: Measuring")!=std::string::npos);
    s=base;s.hosts[0].diagnostics.routes[0].rates.send_hz=0;assert(TopicDetail(s).find("Network Send Hz: 0.00")!=std::string::npos);
    s.hosts[0].diagnostics.routes[0].state=3;assert(TopicDetail(s).find("Network Send Hz: Unavailable")!=std::string::npos);
    for(auto state:{DiagnosticsState::UNAVAILABLE,DiagnosticsState::UNSUPPORTED,DiagnosticsState::INCOMPATIBLE,DiagnosticsState::PENDING,DiagnosticsState::ERROR,DiagnosticsState::TIMEOUT,DiagnosticsState::LOST}){
        s=base;s.hosts[0].diagnostics.status=state;s.hosts[0].diagnostics.error=-110;text=TopicDetail(s);
        assert(text.find("packets_sent=101")==std::string::npos&&text.find("42.50")==std::string::npos);
        if(state==DiagnosticsState::TIMEOUT)assert(text.find("Query timeout")!=std::string::npos);
        if(state==DiagnosticsState::ERROR)assert(text.find("Query error")!=std::string::npos);
        if(state==DiagnosticsState::UNSUPPORTED)assert(text.find("capability unsupported")!=std::string::npos);
    }
    for(int field=0;field<4;++field){s=base;if(field==0)s.hosts[0].state="LOST";if(field==1)++s.hosts[0].session;
        if(field==2)s.hosts[0].diagnostics.gateway_host+="x";if(field==3)s.hosts[0].diagnostics.gateway_boot+="x";
        text=TopicDetail(s);assert(text.find("Unavailable")!=std::string::npos&&text.find("packets_sent=101")==std::string::npos);
    }
    s=base;s.hosts[0].diagnostics.status=DiagnosticsState::TRUNCATED;s.hosts[0].diagnostics.truncated=true;s.hosts[0].diagnostics.total_routes=5;
    text=TopicDetail(s);assert(text.find("Truncated snapshot")!=std::string::npos&&text.find("Network Send Hz: 42.50")!=std::string::npos);
    s.hosts[0].diagnostics.routes.clear();assert(TopicDetail(s).find("absence is unknown")!=std::string::npos);
    s=base;s.hosts[0].diagnostics.routes.clear();s.hosts[0].diagnostics.total_routes=0;assert(TopicDetail(s).find("No matching diagnostics route")!=std::string::npos);
    GraphWidget graph;ElementInfo local;local.identity={777,1};local.name="local";graph.SetSnapshot({}, {local}, {}, {}, {}, base);graph.show();QApplication::processEvents();
    auto* view=graph.findChild<QGraphicsView*>("graphView");auto* detail=graph.findChild<QPlainTextEdit*>("graphRemoteDetail");
    const auto select=[&](const char* id){view->scene()->clearSelection();for(auto* item:view->scene()->items())if(item->data(0).toString()==id){item->setSelected(true);return;}assert(false);};
    select("remote-endpoint:host0:boot:100:123:7");assert(detail->toPlainText().contains("Network Send Hz: 42.50"));
    view->scene()->clearSelection();assert(detail->toPlainText().isEmpty());
    select("remote-endpoint:host0:boot:100:123:7");select("element:777:1");assert(detail->toPlainText().isEmpty()&&detail->isHidden());
    s=base;s.hosts[0].diagnostics.status=DiagnosticsState::ERROR;s.hosts[0].diagnostics.error=-5;
    graph.SetSnapshot({}, {local}, {}, {}, {}, s);select("remote-endpoint:host0:boot:100:123:7");
    assert(detail->toPlainText().contains("Query error")&&!detail->toPlainText().contains("42.50"));
}
NetworkSnapshot VerifiedSnapshot(){
    auto s=DiagnosticsSnapshot();auto& tx=s.hosts[0];tx.endpoints.resize(1);
    tx.diagnostics.observed_at_unix_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto rx=tx;rx.id="other";rx.boot_id="boot2";rx.session=2;
    auto& e=rx.endpoints[0];e.id="other:boot2:200:234:8";e.runtime_id="other:boot2:200:234";e.application_id="other-app";e.role="SUBSCRIBER";
    rx.diagnostics.gateway_host=rx.id;rx.diagnostics.gateway_boot=rx.boot_id;rx.diagnostics.gateway_session=rx.session;rx.diagnostics.routes[0].sending=false;
    s.hosts.push_back(rx);return s;
}
std::vector<GraphEdge> VerifiedEdges(const NetworkSnapshot& s,const GraphOptions& options={}){
    std::vector<GraphEdge> result;auto m=AddRemoteGraph({},s,options);
    std::set<std::string> nodes;for(const auto& n:m.nodes)nodes.insert(n.id);
    for(const auto& e:m.edges){assert(nodes.count(e.from)&&nodes.count(e.to));if(e.network_route)result.push_back(e);}return result;
}
void VerifiedRoutes(){
    const auto base=VerifiedSnapshot();auto edges=VerifiedEdges(base);assert(edges.size()==1);
    assert(edges[0].from=="remote-endpoint:host0:boot:100:123:7"&&edges[0].to=="remote-endpoint:other:boot2:200:234:8");
    assert(edges[0].detail.find("Network Send Hz: 42.50")!=std::string::npos&&edges[0].detail.find("Network Receive Hz: 41.50")!=std::string::npos);
    // Either side must provide current evidence, metadata, and the exact identity tuple.
    for(int side=0;side<2;++side)for(int mutation=0;mutation<29;++mutation){
        auto s=base;auto& h=s.hosts[side];auto& d=h.diagnostics;auto& r=d.routes[0];
        switch(mutation){
        case 0:d.routes.clear();break;case 1:h.state="LOST";break;case 2:h.session++;break;
        case 3:d.gateway_host+="x";break;case 4:d.gateway_boot+="x";break;case 5:d.gateway_session++;break;
        case 6:d.status=DiagnosticsState::ERROR;break;case 7:d.status=DiagnosticsState::TIMEOUT;break;
        case 8:d.observed_at_unix_ms-=10001;break;case 9:d.observed_at_unix_ms=0;break;
        case 10:d.observed_at_unix_ms+=60000;break;case 11:r.route_id++;break;
        case 12:r.source_session++;break;case 13:r.destination_session++;break;
        case 14:r.source.registration_id++;break;case 15:r.destination.start_ticks++;break;
        case 16:r.state=0;break;case 17:r.state=2;break;case 18:r.state=3;break;
        case 19:r.sending=!r.sending;break;case 20:h.endpoints.clear();break;
        case 21:h.endpoints[0].role="OWNER";break;case 22:h.endpoints[0].id+="x";break;
        case 23:h.compatibility="INCOMPATIBLE";break;case 24:d.status=DiagnosticsState::UNSUPPORTED;break;
        case 25:r.source.boot_id+="x";break;case 26:r.destination.pid++;break;
        case 27:h.metadata_ready=false;break;case 28:h.boot_id+="x";break;
        }assert(VerifiedEdges(s).empty());
    }
    auto s=base;for(auto& h:s.hosts){h.diagnostics.status=DiagnosticsState::TRUNCATED;h.diagnostics.truncated=true;h.diagnostics.total_routes=100;}
    assert(VerifiedEdges(s).size()==1);s.hosts[1].diagnostics.routes.clear();assert(VerifiedEdges(s).empty());
    s=base;for(auto& h:s.hosts){auto r=h.diagnostics.routes[0];h.diagnostics.routes.push_back(r);r.route_id++;h.diagnostics.routes.push_back(r);}
    assert(VerifiedEdges(s).size()==2);
    s=base;s.hosts[0].diagnostics.routes[0].rates.status=NetworkRateState::MEASURING;s.hosts[1].diagnostics.routes[0].rates.status=NetworkRateState::INVALID;
    edges=VerifiedEdges(s);assert(edges[0].detail.find("Hz: Measuring")!=std::string::npos&&edges[0].detail.find("Hz: Unavailable")!=std::string::npos);
    s.hosts[0].diagnostics.routes[0].rates.status=NetworkRateState::VALID;s.hosts[0].diagnostics.routes[0].rates.send_hz=0;
    assert(VerifiedEdges(s)[0].detail.find("Send Hz: 0.00")!=std::string::npos);
    for(int filter=0;filter<6;++filter){GraphOptions o;
        if(filter==0)o.host="host0";if(filter==1)o.host="other";if(filter==2)o.group="remote-app:other-app";
        if(filter==3)o.topics=false;if(filter==4)o.remote=false;if(filter==5)o.host="local";
        assert(VerifiedEdges(base,o).empty());
    }
    GraphOptions remote;remote.local=false;assert(VerifiedEdges(base,remote).size()==1);
    GraphWidget graph;graph.SetSnapshot({}, {}, {}, {}, {}, base);graph.show();QApplication::processEvents();
    auto* view=graph.findChild<QGraphicsView*>("graphView");int routes=0;
    for(auto* item:view->scene()->items())if(auto* path=dynamic_cast<QGraphicsPathItem*>(item)){
        if(path->toolTip().contains("route_id=15")){++routes;assert(path->pen().style()==Qt::SolidLine&&path->pen().color()==QColor("#007f9e"));}
        else assert(path->pen().style()==Qt::DashLine);
    }assert(routes==1);
    view->scale(1.2,1.2);const auto zoom=view->transform();
    auto* host=graph.findChild<QComboBox*>("graphHostFilter");host->setCurrentIndex(host->findData("host0"));
    for(const auto& e:graph.Model().edges)assert(!e.network_route);assert(view->transform()==zoom);
    host->setCurrentIndex(0);assert(view->dragMode()==QGraphicsView::ScrollHandDrag);
    graph.findChild<QPushButton*>("graphFit")->click();assert(view->transform().isInvertible());
    s=base;s.hosts[1].diagnostics.routes.clear();graph.SetSnapshot({}, {}, {}, {}, {}, s);
    for(const auto& e:graph.Model().edges)assert(!e.network_route);
}
int main(int argc,char** argv){
    QApplication app(argc,argv);DiagnosticsDetails();VerifiedRoutes();auto s=Snapshot();GraphOptions all{true,true,true,{},true};
    auto m=AddRemoteGraph({},s,all);assert(m.nodes.size()==12 && m.edges.size()==8);
    std::map<std::string,std::string> hosts;for(const auto& n:m.nodes)assert(hosts.emplace(n.id,n.host).second);
    // Same-name metadata never creates cross-host/control or inferred proxy edges.
    for(const auto& e:m.edges)assert(hosts.at(e.from)==hosts.at(e.to));
    assert(m.nodes[0].detail.find("Capabilities")!=std::string::npos);
    assert(m.nodes[6].label.find("LOST")!=std::string::npos && m.nodes[6].label.find("INCOMPATIBLE")!=std::string::npos);
    auto filtered=all;filtered.host="host0";assert(AddRemoteGraph({},s,filtered).nodes.size()==6);
    filtered=all;filtered.remote=false;assert(AddRemoteGraph({},s,filtered).nodes.empty());
    filtered=all;filtered.actions=false;assert(AddRemoteGraph({},s,filtered).nodes.size()==10);
    s.hosts[0].session=2;s.hosts[0].revision=3;s.hosts[0].endpoints.clear();
    m=AddRemoteGraph({},s,all);assert(m.nodes.size()==7 && m.edges.size()==4); // no stale endpoints
    s=Snapshot();auto one=s;one.hosts.resize(1);assert(AddRemoteGraph({},one,all).nodes.size()==6);
    GraphWidget graph;graph.SetSnapshot({}, {}, {}, {}, {}, s);
    int local_calls=0;graph.open_element=[&](auto){++local_calls;};graph.open_topic=[&](auto){++local_calls;};
    graph.show();app.processEvents();
    auto* view=graph.findChild<QGraphicsView*>("graphView");view->scale(1.2,1.2);const auto zoom=view->transform();
    const auto activate=[&](const std::string& id){view->scene()->clearSelection();
        for(auto* item:view->scene()->items())if(item->data(0).toString()==QString::fromStdString(id)){
            item->setSelected(true);QKeyEvent key(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QApplication::sendEvent(view,&key);return;}assert(false);};
    activate("remote-endpoint:host0:boot:100:123:T");assert(local_calls==0);
    auto* detail=graph.findChild<QPlainTextEdit*>("graphRemoteDetail");assert(detail->isReadOnly());
    assert(detail->toPlainText().contains("Endpoint identity: host0") && detail->toPlainText().contains("API unavailable"));
    // Detail must describe the current selection, never a previously selected peer.
    view->scene()->clearSelection();
    assert(detail->toPlainText().isEmpty() && detail->isHidden());
    ElementInfo local;local.identity={777,1};local.name="local-element";
    graph.SetSnapshot({}, {local}, {}, {}, {}, s);
    activate("remote-endpoint:host0:boot:100:123:T");
    for(auto* item:view->scene()->items())if(item->data(0).toString()=="element:777:1"){
        item->setSelected(true);break;
    }
    // Use a single Local selection, as with a plain click in the view.
    for(auto* item:view->scene()->selectedItems())
        if(item->data(0).toString()!="element:777:1")item->setSelected(false);
    assert(detail->toPlainText().isEmpty() && detail->isHidden());
    assert(local_calls==0);
    graph.findChild<QCheckBox*>("graphActions")->setChecked(true);assert(view->transform()==zoom);
    auto* host_filter=graph.findChild<QComboBox*>("graphHostFilter");host_filter->setCurrentIndex(host_filter->findData("host1"));
    for(const auto& node:graph.Model().nodes)assert(node.host=="host1");
    graph.SetSnapshot({}, {}, {}, {}, {}, {});assert(detail->toPlainText().isEmpty());assert(host_filter->currentIndex()==0);
    assert(view->transform()==zoom && view->dragMode()==QGraphicsView::ScrollHandDrag);
}
