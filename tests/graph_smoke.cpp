#include "kcf_tool/ui/main_window.hpp"
#include "kcf_tool/ui/graph_widget.hpp"
#include "kcf_tool/backend/mock_backend.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QMenu>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLabel>
#include <QAction>
#include <QTreeWidget>
#include <QScrollBar>
#include <QElapsedTimer>
#include <thread>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <map>
#include <set>
using namespace kcf_tool;
struct Discovery final:ToolBackend {
    std::vector<ElementInfo> elements;
    std::vector<TopicInfo> topics;
    int refreshes=0,error=0;
    BackendConnectionState connection=BackendConnectionState::CONNECTED;
    NetworkSnapshot network;
    std::vector<RemoteCommand> commands;
    std::unique_ptr<std::promise<RemoteReply>> subscribe;
    std::future<RemoteReply> SubmitRemote(const RemoteCommand& command)override{
        commands.push_back(command);
        if(command.operation==RemoteOperation::Subscribe){subscribe=std::make_unique<std::promise<RemoteReply>>();return subscribe->get_future();}
        std::promise<RemoteReply> reply;reply.set_value({});return reply.get_future();
    }
    std::vector<ServiceInfo> services;
    Discovery(){
        for(int i=0;i<3;++i){ElementInfo e;e.name="same";e.pid=100+i;e.identity={e.pid,1};if(i<2)e.supervisor={50+i,1};elements.push_back(e);}
        for(int i=0;i<2;++i){TopicInfo t;t.name="/shared";t.identity={{100+i,1},1};t.role=i?"SUBSCRIBER":"PUBLISHER";topics.push_back(t);}
    }
    BackendConnectionState GetConnectionState()const override{return connection;}
    NetworkSnapshot GetNetworkSnapshot()override{return network;}
    std::vector<ServiceInfo> QueryServices()override{return services;}
    ApplicationInfo GetApplicationInfo()override{return {"same","RUNNING",{50,1}};}
    std::vector<ApplicationInfo> GetApplications()override{return {{"same","RUNNING",{50,1}},{"same","RUNNING",{51,1}}};}
    std::vector<ElementInfo> GetElements()override{return elements;}
    std::vector<TopicInfo> GetTopics()override{return topics;}
    std::vector<ParameterInfo> GetParameters()override{return {};}
    int Refresh()override{++refreshes;return error;}
    int GetTopicInfo(const std::string&,TopicInfo&)override{return -ENOTSUP;}
    int ReadTopicLatest(const std::string&,DataSnapshot&)override{return -ENOTSUP;}
    int StartTopicMonitor(const std::string&,TopicDataCallback)override{return -ENOTSUP;}
    int StopTopicMonitor(const std::string&)override{return -ENOTSUP;}
    int GetParameter(const std::string&,ParameterInfo&)override{return -ENOTSUP;}
    int SetParameter(const std::string&,const std::string&)override{return -ENOTSUP;}
};
GraphWidget* Graph(MainWindow& window){return dynamic_cast<GraphWidget*>(window.findChild<QWidget*>("graphTab"));}
std::map<QString,QPointF> Positions(QGraphicsScene* scene){std::map<QString,QPointF> result;
    for(auto* item:scene->items())if(item->data(0).isValid())result[item->data(0).toString()]=item->pos();return result;}
template<class F>void UiWaitAt(F predicate,int line){QElapsedTimer clock;clock.start();while(!predicate()){if(clock.elapsed()>=4000){std::cerr<<"UI wait failed at line "<<line<<std::endl;assert(false);}QApplication::processEvents();std::this_thread::sleep_for(std::chrono::milliseconds(10));}}
#define UiWait(...) UiWaitAt((__VA_ARGS__),__LINE__)
void RemoteMetadata(){
    auto backend=std::make_unique<Discovery>();auto* data=backend.get();
    auto& network=data->network;network.observer_host="observer";network.network_client_available=true;network.remote_control_available=true;
    RemoteHost h;h.id="host";h.boot_id="boot";h.session=1;h.state="ONLINE";h.compatibility="COMPATIBLE";h.metadata_ready=true;
    RemoteEndpoint e;e.id="host:boot:100:1:1";e.identity={h.id,h.boot_id,100,1,1};e.session=1;e.runtime_id="runtime";e.application_id="app";e.application="App";e.element="Element";e.kind="T";e.role="PUBLISHER";e.scope="REMOTE";e.name="/topic";
    h.endpoints={e};
    for(const std::string kind:{"P","S","A"}){auto endpoint=e;endpoint.kind=kind;endpoint.role=kind=="P"?"OWNER":"SERVER";endpoint.id+=kind;endpoint.name+="/"+kind;h.endpoints.push_back(endpoint);}
    h.last_complete_snapshot={true,false,1,h.endpoints};network.hosts={h};
    MainWindow window(std::move(backend));window.show();
    auto* selector=window.findChild<QComboBox*>("remoteHostSelector");UiWait([&]{return selector->findData("host")>=0;});selector->setCurrentIndex(selector->findData("host"));
    auto* list=window.findChild<QListWidget*>("remoteEndpoints2");auto* tree=window.findChild<QTreeWidget*>("remoteApplications");
    auto button=[&](const QString& text){for(auto* b:window.findChildren<QPushButton*>())if(b->text()==text)return b;assert(false);return static_cast<QPushButton*>(nullptr);};
    auto* sub=button("Subscribe");auto* unsub=button("Unsubscribe");list->setCurrentRow(0);UiWait([&]{return sub->isEnabled();});
    auto* item=list->currentItem();auto* root=tree->topLevelItem(0);root->setExpanded(true);
    const auto count=[&](RemoteOperation op){return std::count_if(data->commands.begin(),data->commands.end(),[&](const auto& c){return c.operation==op;});};
    for(int page:{3,5,7})window.findChild<QListWidget*>(QString("remoteEndpoints%1").arg(page))->setCurrentRow(0);
    sub->click();assert(data->subscribe);auto& peer=network.hosts[0];peer.metadata_ready=false;peer.endpoints.clear();peer.last_complete_snapshot.stale=true;
    RemoteReply reply;reply.handle=41;data->subscribe->set_value(reply);
    UiWait([&]{return list->toolTip().contains("STALE")&&unsub->isEnabled();});
    assert(list->currentItem()==item&&list->count()==1&&tree->topLevelItem(0)==root&&root->isExpanded());
    assert(!sub->isEnabled()&&count(RemoteOperation::Unsubscribe)==0);
    // Bypass disabled-widget behavior: submission itself must reject stale data.
    QMetaObject::invokeMethod(sub,"clicked",Qt::DirectConnection);assert(count(RemoteOperation::Subscribe)==1);
    for(const QString& text:{QString("Get / Revert"),QString("Set + Readback"),QString("Load request fields"),QString("Call once"),QString("Load goal fields"),QString("Start Goal")}){
        auto* control=button(text);assert(!control->isEnabled());const auto before=data->commands.size();QMetaObject::invokeMethod(control,"clicked",Qt::DirectConnection);assert(data->commands.size()==before);
    }
    unsub->click();UiWait([&]{return !unsub->isEnabled();});assert(count(RemoteOperation::Unsubscribe)==1&&std::any_of(data->commands.begin(),data->commands.end(),[](const auto& c){return c.operation==RemoteOperation::Unsubscribe&&c.handle==41;}));
    peer=h;UiWait([&]{return sub->isEnabled();});assert(list->currentItem()==item);
    // Check the submission gate against a fresh backend snapshot before a UI tick.
    peer.metadata_ready=false;peer.endpoints.clear();peer.last_complete_snapshot.stale=true;
    QMetaObject::invokeMethod(sub,"clicked",Qt::DirectConnection);assert(count(RemoteOperation::Subscribe)==1);
    peer=h;sub->click();assert(count(RemoteOperation::Subscribe)==2);
    peer.endpoints[0].id="host:boot:100:1:2";peer.endpoints[0].identity.registration_id=2;
    peer.last_complete_snapshot.endpoints=peer.endpoints;
    UiWait([&]{return list->currentRow()==-1;});reply.handle=42;data->subscribe->set_value(reply);
    UiWait([&]{return count(RemoteOperation::Unsubscribe)==2;});assert(!unsub->isEnabled());
    list->setCurrentRow(0);UiWait([&]{return sub->isEnabled();});
    peer.endpoints.clear();peer.last_complete_snapshot={true,false,3,{}};
    UiWait([&]{return list->count()==0;});assert(list->currentRow()==-1);
    // Invalid, old-Core absent snapshot, LOST, and generation changes clear selection.
    for(int mode=0;mode<5;++mode){
        peer=h;UiWait([&]{return list->count()==1&&list->item(0)->data(Qt::UserRole).toString()==QString::fromStdString(e.id)+":1";});list->setCurrentRow(0);UiWait([&]{return sub->isEnabled();});
        if(mode==0){peer.metadata_ready=false;peer.endpoints.clear();peer.last_complete_snapshot={};}
        if(mode==1)peer.state="LOST";
        if(mode==2){peer.boot_id="new-boot";peer.endpoints.clear();peer.metadata_ready=false;peer.last_complete_snapshot.stale=true;}
        if(mode==3){peer.session=2;peer.endpoints[0].session=2;peer.last_complete_snapshot={};}
        if(mode==4){network.hosts.clear();network.status="Discovery error";}
        UiWait([&]{return list->currentRow()==-1&&!sub->isEnabled();});
        if(mode==4){network.hosts={h};break;} // vector reference is no longer valid
    }
    // Observer generation can change while endpoints retain identical identity.
    network.hosts={h};UiWait([&]{return list->count()==1&&list->item(0)->data(Qt::UserRole).toString()==QString::fromStdString(e.id)+":1";});list->setCurrentRow(0);UiWait([&]{return sub->isEnabled();});network.observer_host="new-observer";
    UiWait([&]{return list->currentRow()==-1;});assert(count(RemoteOperation::Unsubscribe)==2);
}
void CommunicationView(){
    GraphWidget widget;widget.resize(700,350);widget.show();
    std::vector<ElementInfo> elements;
    for(int pid:{1,2,3}){ElementInfo e;e.identity={pid,10};e.name="same";elements.push_back(e);}
    std::vector<TopicInfo> topics;
    for(const std::string name:{"/one","/two"})for(int pid:{1,2,3}){
        TopicInfo t;t.name=name;t.role=pid==1?"PUBLISHER":"SUBSCRIBER";t.identity={{pid,10},name=="/one"?1u:2u};topics.push_back(t);
    }
    widget.SetSnapshot({},elements,topics,{},{});
    auto* mode=widget.findChild<QComboBox*>("graphMode");assert(mode&&mode->currentText()=="Ownership");
    const auto ownership_edges=widget.Model().edges.size();assert(ownership_edges==6);
    auto* view=widget.findChild<QGraphicsView*>("graphView");auto* scene=view->scene();
    for(auto* item:scene->items())if(item->data(0).toString()=="element:1:10")item->setSelected(true);
    widget.findChild<QPushButton*>("graphZoomIn")->click();const auto zoom=view->transform();
    mode->setCurrentIndex(1);assert(widget.CommunicationModel().edges.size()==4&&widget.Model().nodes.size()==3&&widget.Model().edges.empty());
    assert(view->transform()==zoom&&scene->selectedItems().size()==1);
    int edges=0,arrows=0,labels=0;std::set<QString> names;std::set<qreal> lanes;
    for(auto* item:scene->items()){
        const auto kind=item->data(2).toString();
        if(kind=="communication-metadata"){
            ++edges;auto* line=dynamic_cast<QGraphicsPathItem*>(item);assert(line&&line->pen().style()==Qt::DashLine);
            assert(item->data(3).toString()=="element:1:10");assert(item->data(4).toString()=="element:2:10"||item->data(4).toString()=="element:3:10");
            assert(item->toolTip().contains("NOT verified"));lanes.insert(line->path().elementAt(1).x);
        }
        if(kind=="communication-arrow"){++arrows;auto* arrow=dynamic_cast<QGraphicsPolygonItem*>(item);assert(arrow&&arrow->polygon()[0].x()<arrow->polygon()[1].x());}
        if(kind=="communication-topic"){++labels;names.insert(item->data(5).toString());}
    }
    assert(edges==4&&arrows==4&&labels==4&&lanes.size()==4&&names.size()==2);
    assert(widget.findChild<QLabel*>("graphStatus")->text().contains("Metadata only"));
    QApplication::processEvents();view->horizontalScrollBar()->setValue(40);const int scroll=view->horizontalScrollBar()->value();
    widget.SetSnapshot({},elements,topics,{},{});assert(view->transform()==zoom&&view->horizontalScrollBar()->value()==scroll&&scene->selectedItems().size()==1);
    int refreshes=0;widget.refresh=[&]{++refreshes;widget.SetSnapshot({},elements,topics,{},{});};widget.findChild<QPushButton*>("refreshGraphButton")->click();assert(refreshes==1&&widget.CommunicationModel().edges.size()==4);
    auto* toggle=widget.findChild<QCheckBox*>("graphTopics");toggle->setChecked(false);assert(widget.CommunicationModel().edges.empty());toggle->setChecked(true);
    auto* local=widget.findChild<QCheckBox*>("graphLocal");local->setChecked(false);assert(widget.Model().nodes.empty());local->setChecked(true);
    auto* hosts=widget.findChild<QComboBox*>("graphHostFilter");hosts->addItem("Remote","remote");hosts->setCurrentIndex(hosts->findData("remote"));assert(widget.Model().nodes.empty());hosts->setCurrentIndex(0);
    auto* groups=widget.findChild<QComboBox*>("graphApplicationFilter");groups->addItem("Missing","missing");groups->setCurrentIndex(groups->findData("missing"));assert(widget.CommunicationModel().edges.empty());groups->setCurrentIndex(0);
    RuntimeIdentity activated{};widget.open_element=[&](RuntimeIdentity id){activated=id;};
    for(auto* item:scene->items())if(item->data(0).toString()=="element:1:10")item->setSelected(true);
    QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QApplication::sendEvent(view,&enter);assert(activated.pid==1);
    mode->setCurrentIndex(0);assert(widget.Model().edges.size()==ownership_edges&&widget.CommunicationModel().edges.empty());
    mode->setCurrentIndex(1);widget.SetSnapshot({}, {}, {}, {}, {});assert(widget.Model().nodes.empty()&&widget.CommunicationModel().edges.empty());
}
void RemoteWithoutLocal(){
    auto backend=std::make_unique<Discovery>();auto* data=backend.get();
    data->connection=BackendConnectionState::DISCONNECTED;
    // Leave stale Local data in the backend: the UI must not use it after failure.
    ServiceInfo service;service.name="/stale-service";service.identity={{100,1},9};data->services={service};
    RemoteHost host;host.id="remote-host";host.boot_id="boot";host.label="Remote";
    host.state="ONLINE";host.compatibility="COMPATIBLE";host.session=7;host.metadata_ready=true;
    RemoteEndpoint endpoint;endpoint.id="remote-host:boot:200:2:3";endpoint.runtime_id="remote-host:boot:200:2";
    endpoint.application_id="remote-host:boot:150:2";endpoint.application="remote-app";
    endpoint.element="remote-element";endpoint.name="/remote-topic";endpoint.kind="T";endpoint.role="PUBLISHER";
    endpoint.identity={host.id,host.boot_id,200,2,3};endpoint.session=host.session;
    host.endpoints={endpoint};data->network.hosts={host};
    MainWindow window(std::move(backend));auto* graph=Graph(window);
    auto* refresh=graph->findChild<QPushButton*>("refreshGraphButton");
    auto* local_services=window.findChild<QListWidget*>("servicesList");assert(local_services);
    const auto has=[&](const std::string& id){return std::any_of(graph->Model().nodes.begin(),graph->Model().nodes.end(),
        [&](const auto& node){return node.id==id;});};
    const auto remote_only=[&]{
        assert(has("remote-host:"+host.id)&&has("remote-endpoint:"+data->network.hosts[0].endpoints[0].id));
        assert(graph->Model().nodes.size()==3);
        for(const auto& node:graph->Model().nodes)assert(node.host==host.id);
        for(const auto& edge:graph->Model().edges)assert(!edge.network_route);
        assert(local_services->count()==0);
    };
    remote_only(); // Initial DISCONNECTED + ONLINE metadata.
    auto* hosts=graph->findChild<QComboBox*>("graphHostFilter");
    hosts->setCurrentIndex(hosts->findData("local"));assert(graph->Model().nodes.empty());
    hosts->setCurrentIndex(hosts->findData(QString::fromStdString(host.id)));remote_only();
    auto* topics=graph->findChild<QCheckBox*>("graphTopics");topics->setChecked(false);
    assert(graph->Model().nodes.size()==2&&graph->Model().edges.empty());topics->setChecked(true);remote_only();
    hosts->setCurrentIndex(0);
    // Recover Local discovery, then fail without clearing the backend's cached lists.
    data->connection=BackendConnectionState::CONNECTED;refresh->click();assert(has("element:100:1"));
    assert(local_services->count()==1);
    data->error=-EIO;
    data->network.hosts[0].endpoints[0].id="remote-host:boot:200:2:4";
    data->network.hosts[0].endpoints[0].identity.registration_id=4;
    refresh->click();remote_only();assert(!has("remote-endpoint:"+endpoint.id));
    // A disconnected refresh also applies the latest snapshot, not a retained copy.
    data->error=0;data->connection=BackendConnectionState::DISCONNECTED;
    data->network.hosts[0].endpoints[0]=endpoint;
    window.findChild<QPushButton*>("refreshButton")->click();remote_only();
    data->network={};refresh->click();
    assert(graph->Model().nodes.empty()&&graph->Model().edges.empty());
    assert(graph->findChild<QGraphicsView*>("graphView")->scene()->items().empty());
}
int main(int argc,char** argv){
    QApplication app(argc,argv);RemoteWithoutLocal();RemoteMetadata();CommunicationView();auto backend=std::make_unique<Discovery>();auto* data=backend.get();MainWindow window(std::move(backend));window.show();
    auto* graph=Graph(window);assert(graph && graph->Model().nodes.size()==4 && graph->Model().edges.size()==2);
    auto* tabs=window.findChild<QTabWidget*>();assert(tabs->tabText(6)=="Graph");tabs->setCurrentWidget(graph);app.processEvents();
    auto* view=graph->findChild<QGraphicsView*>("graphView");auto* scene=view->scene();auto* refresh=graph->findChild<QPushButton*>("refreshGraphButton");
    const auto first=Positions(scene);std::reverse(data->elements.begin(),data->elements.end());std::reverse(data->topics.begin(),data->topics.end());
    refresh->click();assert(Positions(scene)==first);
    const int reads=data->refreshes;
    auto* toggle=graph->findChild<QCheckBox*>("graphTopics");toggle->setChecked(false);assert(graph->Model().nodes.size()==3 && graph->Model().edges.empty());toggle->setChecked(true);
    auto* filter=graph->findChild<QComboBox*>("graphApplicationFilter");assert(filter->count()==4);
    filter->setCurrentIndex(filter->findData("51:1"));assert(graph->Model().nodes.size()==2 && graph->Model().edges.size()==1);
    filter->setCurrentIndex(0);assert(data->refreshes==reads);
    const auto scale=view->transform().m11();graph->findChild<QPushButton*>("graphZoomIn")->click();assert(view->transform().m11()>scale);
    graph->findChild<QPushButton*>("graphZoomOut")->click();assert(qAbs(view->transform().m11()-scale)<0.001);
    view->scale(0.01,0.01); // Fit on a large graph may place scale below the normal wheel range.
    graph->findChild<QPushButton*>("graphZoomIn")->click();assert(view->transform().m11()>=0.15);
    view->resetTransform();
    assert(view->dragMode()==QGraphicsView::ScrollHandDrag);
    const auto activate=[&](const QString& id){
        scene->clearSelection();for(auto* item:scene->items())if(item->data(0).toString()==id){
            item->setSelected(true);QKeyEvent key(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QApplication::sendEvent(view,&key);return;}
        assert(false);
    };
    activate("element:101:1");assert(tabs->currentIndex()==1);
    assert(window.findChild<QLabel*>("elementIdentity")->text().contains("PID 101"));
    tabs->setCurrentWidget(graph);activate("1:/shared");auto* menu=graph->findChild<QMenu*>();assert(menu && menu->actions().size()==2);
    for(auto* action:menu->actions())if(action->text().startsWith("SUBSCRIBER"))action->trigger();menu->close();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);app.processEvents();
    assert(tabs->currentIndex()==2 && window.findChild<QListWidget*>("topicsList")->currentItem()->text().contains("SUBSCRIBER PID 101"));
    tabs->setCurrentWidget(graph);activate("1:/shared");menu=graph->findChild<QMenu*>();assert(menu);
    data->topics.clear();refresh->click();assert(graph->Model().nodes.size()==3 && graph->Model().edges.empty());
    menu->actions().front()->trigger();assert(tabs->currentIndex()==6);menu->close();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);app.processEvents();
    TopicInfo added;added.name="/added";added.identity={{102,1},4};added.role="PUBLISHER";data->topics.push_back(added);refresh->click();
    activate("1:/added");assert(tabs->currentIndex()==2 && window.findChild<QListWidget*>("topicsList")->currentItem()->text().startsWith("/added"));
    filter->setCurrentIndex(filter->findData("51:1"));data->elements.clear();data->error=-EIO;refresh->click();
    assert(graph->Model().nodes.empty() && scene->items().empty() && filter->currentIndex()==0);
    // Existing Mock backend: no names inferred from publisher/subscriber text.
    MainWindow mock(std::make_unique<MockBackend>());auto* mg=Graph(mock);assert(mg->Model().edges.size()==3);
    mg->findChild<QCheckBox*>("graphParameters")->setChecked(true);mg->findChild<QCheckBox*>("graphServices")->setChecked(true);
    assert(mg->Model().edges.size()==8);
    auto* params=mock.findChild<QListWidget*>("parametersList");params->setCurrentRow(0);
    mock.findChild<QLineEdit*>("parameterEditor")->setText("42");
    auto* mtabs=mock.findChild<QTabWidget*>();mtabs->setCurrentWidget(mg);
    mg->open_element({12001,1});assert(mtabs->currentWidget()==mg); // unsaved edits protected
    mock.findChild<QPushButton*>("revertParameterButton")->click();
}
