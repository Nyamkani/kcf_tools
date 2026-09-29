#include "kcf_tool/ui/main_window.hpp"
#include "kcf_tool/ui/graph_widget.hpp"
#include "kcf_tool/backend/mock_backend.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGraphicsView>
#include <QGraphicsItem>
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
#include <algorithm>
#include <cassert>
#include <map>
using namespace kcf_tool;
struct Discovery final:ToolBackend {
    std::vector<ElementInfo> elements;
    std::vector<TopicInfo> topics;
    int refreshes=0,error=0;
    Discovery(){
        for(int i=0;i<3;++i){ElementInfo e;e.name="same";e.pid=100+i;e.identity={e.pid,1};if(i<2)e.supervisor={50+i,1};elements.push_back(e);}
        for(int i=0;i<2;++i){TopicInfo t;t.name="/shared";t.identity={{100+i,1},1};t.role=i?"SUBSCRIBER":"PUBLISHER";topics.push_back(t);}
    }
    BackendConnectionState GetConnectionState()const override{return BackendConnectionState::CONNECTED;}
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
int main(int argc,char** argv){
    QApplication app(argc,argv);auto backend=std::make_unique<Discovery>();auto* data=backend.get();MainWindow window(std::move(backend));window.show();
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
