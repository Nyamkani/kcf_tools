#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf_tool/ui/main_window.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QTabWidget>
#include <QThread>
#include <QTreeWidget>
#include <cassert>
#include <csignal>
#include <iostream>
#include <QStatusBar>
#include <QFile>

// Test-only transparent forwarding: observe the actual UI Refresh result without
// an extra discovery scan, readiness gate, retry, or product instrumentation.
namespace kcf_tool {
class ObservedBackend final : public ToolBackend {
    KcfBackend backend_;
public:
    int last_refresh{0}; unsigned refresh_count{0};
    int Refresh() override { ++refresh_count; return last_refresh=backend_.Refresh(); }
    BackendConnectionState GetConnectionState() const override { return backend_.GetConnectionState(); }
    ApplicationInfo GetApplicationInfo() override { return backend_.GetApplicationInfo(); }
    std::vector<ApplicationInfo> GetApplications() override { return backend_.GetApplications(); }
    std::vector<ElementInfo> GetElements() override { return backend_.GetElements(); }
    std::vector<TopicInfo> GetTopics() override { return backend_.GetTopics(); }
    std::vector<ParameterInfo> GetParameters() override { return backend_.GetParameters(); }
    std::vector<ServiceInfo> QueryServices() override { return backend_.QueryServices(); }
    int GetTopicInfo(const std::string& a, TopicInfo& b) override { return backend_.GetTopicInfo(a,b); }
    int ReadTopicLatest(const std::string& a, DataSnapshot& b) override { return backend_.ReadTopicLatest(a,b); }
    int StartTopicMonitor(const std::string& a, TopicDataCallback b) override { return backend_.StartTopicMonitor(a,b); }
    int StopTopicMonitor(const std::string& a) override { return backend_.StopTopicMonitor(a); }
    int GetParameter(const std::string& a, ParameterInfo& b) override { return backend_.GetParameter(a,b); }
    int SetParameter(const std::string& a, const std::string& b) override { return backend_.SetParameter(a,b); }
    int StartTopicEcho(const EndpointIdentity& a) override { return backend_.StartTopicEcho(a); }
    int ReadTopicEcho(DataSnapshot& a) override { return backend_.ReadTopicEcho(a); }
    void StopTopicEcho() override { return backend_.StopTopicEcho(); }
    int PrepareTopicRead(const EndpointIdentity& a) override { return backend_.PrepareTopicRead(a); }
    void CloseTopicRead() override { return backend_.CloseTopicRead(); }
    int ReadTopicLatest(const EndpointIdentity& a, DataSnapshot& b) override { return backend_.ReadTopicLatest(a,b); }
    int GetParameter(const EndpointIdentity& a, DataSnapshot& b) override { return backend_.GetParameter(a,b); }
    int ValidateParameter(const EndpointIdentity& a, const DataSnapshot& b) override { return backend_.ValidateParameter(a,b); }
    int SetParameter(const EndpointIdentity& a, const DataSnapshot& b) override { return backend_.SetParameter(a,b); }
    int GetTypeTemplate(const RuntimeIdentity& a, std::uint64_t b, DataSnapshot& c) override { return backend_.GetTypeTemplate(a,b,c); }
    int ValidateService(const EndpointIdentity& a, const DataSnapshot& b) override { return backend_.ValidateService(a,b); }
    int CallService(const EndpointIdentity& a, const DataSnapshot& b, DataSnapshot& c, std::uint32_t d, std::uint32_t e) override { return backend_.CallService(a,b,c,d,e); }
    int ConfigureLocalGateway(const std::string& a,bool b) override { return backend_.ConfigureLocalGateway(a,b); }
    LocalGatewaySnapshot GetLocalGatewaySnapshot() override { return backend_.GetLocalGatewaySnapshot(); }
    int ConfigureNetwork(const NetworkOptions& a,bool b) override { return backend_.ConfigureNetwork(a,b); }
    NetworkOptions GetNetworkOptions() const override { return backend_.GetNetworkOptions(); }
    NetworkSnapshot GetNetworkSnapshot() override { return backend_.GetNetworkSnapshot(); }
    std::future<RemoteReply> SubmitRemote(const RemoteCommand& a) override { return backend_.SubmitRemote(a); }
};
}

int main(int argc,char** argv){
    QApplication app(argc,argv);assert(argc==2);
    const QString prefix="/kt7_"+QString::number(QCoreApplication::applicationPid());
    const QString nameA=prefix+"_A",nameB=prefix+"_B";
    QProcess a,b,standalone;
    a.setProcessChannelMode(QProcess::MergedChannels);b.setProcessChannelMode(QProcess::MergedChannels);standalone.setProcessChannelMode(QProcess::MergedChannels);
    a.start(argv[1],{"--application",nameA,prefix+"/a"});assert(a.waitForStarted());
    standalone.start(argv[1],{"--element",prefix+"/d"});assert(standalone.waitForStarted());
    auto backend=std::make_unique<kcf_tool::ObservedBackend>();auto* observed=backend.get();
    kcf_tool::MainWindow window(std::move(backend),"KCF");window.show();
    auto* refresh=window.findChild<QPushButton*>("refreshButton");
    auto* tree=window.findChild<QTreeWidget*>("applicationsTree");
    auto* endpoints=window.findChild<QTreeWidget*>("elementEndpoints");
    auto* elements=window.findChild<QListWidget*>("elementsList");
    auto* tabs=window.findChild<QTabWidget*>();
    const auto group=[&](const QString& name)->QTreeWidgetItem*{
        for(int i=0;i<tree->topLevelItemCount();++i)if(tree->topLevelItem(i)->text(0)==name)return tree->topLevelItem(i);
        return nullptr;
    };
    const char* phase="initial";
    const auto wait=[&](auto condition){QElapsedTimer timer;timer.start();while(!condition()){
        if(timer.elapsed()>=15000){
            std::cerr<<"FAIL phase="<<phase<<" elapsed_ms="<<timer.elapsed()
                <<" refresh_result="<<observed->last_refresh<<" connection="<<static_cast<int>(observed->GetConnectionState())
                <<" refresh_count="<<observed->refresh_count<<" ui_status="<<window.statusBar()->currentMessage().toStdString()<<'\n';
            for(const auto& name:{nameA,nameB}){
                const auto* item=group(name);
                std::cerr<<"Application name="<<name.toStdString()<<" found="<<(item!=nullptr);
                if(item)std::cerr<<" state/revision="<<item->text(3).toStdString()<<" supervisor_pid="<<item->text(1).toStdString()
                    <<" start_ticks="<<item->text(2).toStdString()<<" managed_children="<<item->childCount();
                std::cerr<<'\n';
            }
            std::cerr<<"standalone="<<(group("Standalone")?group("Standalone")->childCount():-1)
                <<" elements="<<elements->count()<<" services="<<window.findChild<QListWidget*>("servicesList")->count()<<'\n';
            for(const auto& info:observed->GetApplications())std::cerr<<"Backend application="<<info.name<<" state="<<info.state
                <<" pid="<<info.supervisor.pid<<" ticks="<<info.supervisor.process_start_ticks<<" revision="<<info.revision
                <<" managed="<<info.managed_element_count<<'\n';
            for(const auto& info:observed->GetElements())std::cerr<<"Backend runtime pid="<<info.identity.pid
                <<" ticks="<<info.identity.process_start_ticks<<" mode="<<info.mode<<" state="<<info.state
                <<" runtime_error="<<info.runtime_error<<" supervisor="<<info.supervisor.pid<<'\n';
            for(auto* process:{&a,&b,&standalone}){
                std::cerr<<"Fixture "<<(process==&a?"A":process==&b?"B":"Standalone")<<" pid="<<process->processId()
                    <<" state="<<process->state()<<" exit_valid="<<(process->state()==QProcess::NotRunning)
                    <<" exit="<<process->exitCode()<<" exit_status="<<process->exitStatus()<<'\n';
                if(process->isOpen())std::cerr<<process->readAll().toStdString();
                QFile stat("/proc/"+QString::number(process->processId())+"/stat");
                if(stat.open(QIODevice::ReadOnly))std::cerr<<"proc_stat="<<stat.readAll().toStdString();
            }
            std::cerr<<std::flush;assert(false);
        }app.processEvents();QThread::msleep(10);}};
    wait([&]{refresh->click();return group(nameA) && group(nameA)->text(3).startsWith("RUNNING") && group(nameA)->childCount()==1 &&
        group("Standalone") && group("Standalone")->childCount()==1 && elements->count()==2 &&
        window.findChild<QListWidget*>("servicesList")->count()==2;});
    auto* child=group(nameA)->child(0);const auto oldPid=child->text(1),oldTicks=child->text(2);
    assert(group(nameA)->text(1)==QString::number(a.processId()));
    tree->setCurrentItem(child);
    assert(elements->currentRow()>=0);
    assert(window.findChild<QLabel*>("elementIdentity")->text().contains(oldTicks));
    assert(endpoints->topLevelItemCount()==3);
    assert(endpoints->topLevelItem(0)->childCount()==3);
    assert(endpoints->topLevelItem(1)->childCount()==2);
    assert(endpoints->topLevelItem(2)->childCount()==1);
    // Navigation selects the exact existing endpoint row, without copying controls.
    for(int g=0;g<3;++g){auto* item=endpoints->topLevelItem(g)->child(0);
        assert(item->text(0).startsWith(prefix+"/a/"));
        endpoints->itemActivated(item,0);
        assert(tabs->currentIndex()==(g==0?2:g==1?3:5));
        auto* list=window.findChild<QListWidget*>(g==0?"topicsList":g==1?"parametersList":"servicesList");
        assert(list->currentItem()->text().startsWith(item->text(0)));
    }
    refresh->click();assert(tree->currentItem() && tree->currentItem()->text(1)==oldPid);
    // Reset is accepted by the existing Supervisor only in ERROR state.
    phase="enter ERROR";
    assert(::kill(static_cast<pid_t>(oldPid.toInt()),SIGTERM)==0);
    wait([&]{refresh->click();return group(nameA) && group(nameA)->text(3).startsWith("ERROR");});
    phase="reset";
    assert(::kill(static_cast<pid_t>(a.processId()),SIGUSR1)==0);
    wait([&]{refresh->click();auto* parent=group(nameA);return parent && parent->childCount()==1 &&
        (parent->child(0)->text(1)!=oldPid || parent->child(0)->text(2)!=oldTicks);});
    assert(elements->currentRow()==-1 && endpoints->topLevelItemCount()==0 && !tree->currentItem());
    assert(window.findChild<QListWidget*>("topicsList")->currentRow()==-1);
    assert(window.findChild<QListWidget*>("parametersList")->currentRow()==-1);
    assert(window.findChild<QListWidget*>("servicesList")->currentRow()==-1);
    tree->setCurrentItem(group("Standalone")->child(0));
    phase="standalone exit";
    standalone.terminate();assert(standalone.waitForFinished(5000));
    wait([&]{refresh->click();return group("Standalone")->childCount()==0;});
    assert(elements->currentRow()==-1 && endpoints->topLevelItemCount()==0);
    standalone.start(argv[1],{"--element",prefix+"/d"});assert(standalone.waitForStarted());
    wait([&]{refresh->click();return group("Standalone")->childCount()==1;});
    phase="application exit";
    a.terminate();assert(a.waitForFinished(8000));
    wait([&]{refresh->click();return !group(nameA) && elements->count()==1;});
    // Exercise a second Application configuration after the first exits.
    // Application-scoped SystemStatus also supports concurrent Supervisors.
    phase="second application";
    b.start(argv[1],{"--application",nameB,prefix+"/b",prefix+"/c"});assert(b.waitForStarted());
    wait([&]{refresh->click();return group(nameB) && group(nameB)->text(3).startsWith("RUNNING") && group(nameB)->childCount()==2 && elements->count()==3;});
    assert(group(nameB)->text(1)==QString::number(b.processId()));
    b.terminate();standalone.terminate();assert(b.waitForFinished(8000));assert(standalone.waitForFinished(5000));
    wait([&]{refresh->click();return elements->count()==0 && !group(nameA);});
    std::cout<<"PASS explorer phases completed refresh_count="<<observed->refresh_count<<std::endl;
}
