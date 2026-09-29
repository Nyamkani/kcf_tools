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
int main(int argc,char** argv){
    QApplication app(argc,argv);auto s=Snapshot();GraphOptions all{true,true,true,{},true};
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
