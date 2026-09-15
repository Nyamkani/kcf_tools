#include "kcf_tool/ui/main_window.hpp"
#include <QApplication>
#include <QListWidget>
#include <QPushButton>
#include <QTreeWidget>
#include <cassert>

struct Discovery final:kcf_tool::ToolBackend{
    std::vector<kcf_tool::ApplicationInfo> applications{{"same", "RUNNING", {101,1}}, {"same", "RUNNING", {102,2}}};
    std::vector<kcf_tool::ElementInfo> elements;
    int error=0;
    Discovery(){
        for(int i=0;i<3;++i){kcf_tool::ElementInfo e;e.name="same";e.pid=200+i;e.identity={e.pid,10};
            e.supervisor=i==0?kcf_tool::RuntimeIdentity{101,1}:i==1?kcf_tool::RuntimeIdentity{102,2}:kcf_tool::RuntimeIdentity{101,99};
            elements.push_back(e);}
    }
    kcf_tool::BackendConnectionState GetConnectionState()const override{return kcf_tool::BackendConnectionState::CONNECTED;}
    kcf_tool::ApplicationInfo GetApplicationInfo()override{return applications.front();}
    std::vector<kcf_tool::ApplicationInfo> GetApplications()override{return applications;}
    std::vector<kcf_tool::ElementInfo> GetElements()override{return elements;}
    std::vector<kcf_tool::TopicInfo> GetTopics()override{return {};}
    std::vector<kcf_tool::ParameterInfo> GetParameters()override{return {};}
    int Refresh()override{return error;}
    int GetTopicInfo(const std::string&,kcf_tool::TopicInfo&)override{return -ENOTSUP;}
    int ReadTopicLatest(const std::string&,kcf_tool::DataSnapshot&)override{return -ENOTSUP;}
    int StartTopicMonitor(const std::string&,kcf_tool::TopicDataCallback)override{return -ENOTSUP;}
    int StopTopicMonitor(const std::string&)override{return -ENOTSUP;}
    int GetParameter(const std::string&,kcf_tool::ParameterInfo&)override{return -ENOTSUP;}
    int SetParameter(const std::string&,const std::string&)override{return -ENOTSUP;}
};
int main(int argc,char** argv){
    QApplication app(argc,argv);auto backend=std::make_unique<Discovery>();auto* data=backend.get();
    kcf_tool::MainWindow window(std::move(backend));
    auto* tree=window.findChild<QTreeWidget*>("applicationsTree");
    auto* elements=window.findChild<QListWidget*>("elementsList");
    auto* refresh=window.findChild<QPushButton*>("refreshButton");
    assert(tree->topLevelItemCount()==3);
    for(int i=0;i<3;++i){assert(tree->topLevelItem(i)->childCount()==1);
        assert(tree->topLevelItem(i)->child(0)->text(1)==QString::number(200+i));}
    tree->setCurrentItem(tree->topLevelItem(0)->child(0));assert(elements->currentRow()==0);
    std::swap(data->elements[0],data->elements[1]);refresh->click();
    assert(elements->currentRow()==1 && tree->currentItem()->text(1)=="200");
    // PID reuse with a different start tick must not retain a stale selection.
    data->elements[1].identity.process_start_ticks=11;refresh->click();
    assert(elements->currentRow()==-1 && !tree->currentItem());
    refresh->click();assert(elements->currentRow()==-1 && !tree->currentItem());
    // A failing backend may retain its cache: the GUI still clears discovery.
    data->error=-EIO;refresh->click();assert(elements->count()==0);
    assert(tree->topLevelItemCount()==1 && tree->topLevelItem(0)->childCount()==0);
}
