#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf_tool/ui/main_window.hpp"
#include "kcf/dynamic/dynamic_parameter_client.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QTableWidget>
#include <QThread>
#include <cassert>
#include <csignal>
#include <functional>
#include <iostream>

// Wrap public calls only in this test executable, after a real successful Get
// or Set. No Framework hooks or storage internals are used.
std::function<void()> after_get,after_set;
int get_countdown=0;
extern "C" int __real__ZN3kcf22DynamicParameterClient3GetERNS_14DynamicPayloadE(kcf::DynamicParameterClient*,kcf::DynamicPayload&);
extern "C" int __wrap__ZN3kcf22DynamicParameterClient3GetERNS_14DynamicPayloadE(kcf::DynamicParameterClient* self,kcf::DynamicPayload& value){
    const int r=__real__ZN3kcf22DynamicParameterClient3GetERNS_14DynamicPayloadE(self,value);
    if(!r && get_countdown>0 && --get_countdown==0){auto callback=std::move(after_get);after_get={};callback();}return r;
}
extern "C" int __real__ZN3kcf22DynamicParameterClient3SetERKNS_14DynamicPayloadE(kcf::DynamicParameterClient*,const kcf::DynamicPayload&);
extern "C" int __wrap__ZN3kcf22DynamicParameterClient3SetERKNS_14DynamicPayloadE(kcf::DynamicParameterClient* self,const kcf::DynamicPayload& value){
    const int r=__real__ZN3kcf22DynamicParameterClient3SetERKNS_14DynamicPayloadE(self,value);
    if(!r && after_set){auto callback=std::move(after_set);after_set={};callback();}return r;
}
class CountedBackend: public kcf_tool::ToolBackend {
public:
    kcf_tool::KcfBackend real;unsigned sets=0;int last_set=0;
    kcf_tool::BackendConnectionState GetConnectionState()const override{return real.GetConnectionState();}
    kcf_tool::ApplicationInfo GetApplicationInfo()override{return real.GetApplicationInfo();}
    std::vector<kcf_tool::ElementInfo> GetElements()override{return real.GetElements();}
    std::vector<kcf_tool::TopicInfo> GetTopics()override{return real.GetTopics();}
    std::vector<kcf_tool::ParameterInfo> GetParameters()override{return real.GetParameters();}
    int GetTopicInfo(const std::string& n,kcf_tool::TopicInfo& v)override{return real.GetTopicInfo(n,v);}
    int ReadTopicLatest(const std::string& n,kcf_tool::DataSnapshot& v)override{return real.ReadTopicLatest(n,v);}
    int StartTopicMonitor(const std::string& n,kcf_tool::TopicDataCallback c)override{return real.StartTopicMonitor(n,c);}
    int StopTopicMonitor(const std::string& n)override{return real.StopTopicMonitor(n);}
    int GetParameter(const std::string& n,kcf_tool::ParameterInfo& v)override{return real.GetParameter(n,v);}
    int SetParameter(const std::string& n,const std::string& v)override{++sets;return last_set=real.SetParameter(n,v);}
    int GetParameter(const kcf_tool::EndpointIdentity& id,kcf_tool::DataSnapshot& v)override{return real.GetParameter(id,v);}
    int ValidateParameter(const kcf_tool::EndpointIdentity& id,const kcf_tool::DataSnapshot& v)override{return real.ValidateParameter(id,v);}
    int SetParameter(const kcf_tool::EndpointIdentity& id,const kcf_tool::DataSnapshot& v)override{++sets;return last_set=real.SetParameter(id,v);}
    int Refresh()override{return real.Refresh();}
};
std::string Field(const kcf_tool::DataSnapshot& value,const std::string& name){for(const auto& f:value.fields)if(f.name==name)return f.value;assert(false);return {};}
int main(int argc,char** argv){
    QApplication app(argc,argv);assert(argc==2);
    QProcess fixture;const auto prefix="/kt5_"+QString::number(QCoreApplication::applicationPid());
    fixture.start(argv[1],{"--element",prefix});assert(fixture.waitForStarted());
    auto owned=std::make_unique<CountedBackend>();auto* backend=owned.get();
    kcf_tool::MainWindow window(std::move(owned),"KCF");window.show();
    auto* table=window.findChild<QTableWidget*>("parameterFields");
    auto* refresh=window.findChild<QPushButton*>("refreshButton");
    auto* value_refresh=window.findChild<QPushButton*>("refreshParameterButton");
    auto* revert=window.findChild<QPushButton*>("revertParameterButton");
    auto* status=window.findChild<QLabel*>("parameterStatus");
    QPushButton* apply=nullptr;for(auto* b:window.findChildren<QPushButton*>())if(b->text()=="Apply")apply=b;assert(apply);
    const auto wait=[&](auto condition){QElapsedTimer elapsed;elapsed.start();while(!condition()){assert(elapsed.elapsed()<10000);app.processEvents();QThread::msleep(5);}};
    wait([&]{refresh->click();return table->rowCount()==8 && backend->real.QueryServices().size()==1;});
    const auto row=[&](const QString& name){for(int i=0;i<table->rowCount();++i)if(table->item(i,0)->text()==name)return i;assert(false);return -1;};
    const auto input=[&](const QString& name){return qobject_cast<QLineEdit*>(table->cellWidget(row(name),3));};
    const auto current=[&](const QString& name){return table->item(row(name),2)->text();};
    assert(current("gain")=="5" && current("enabled")=="true" && current("values[2]")=="6");
    assert(window.findChild<QLabel*>("parameterMetadata")->text().contains("OWNER"));
    const auto signal=[&](int sig,const QByteArray& marker){
        std::cout<<"Trigger "<<marker.constData()<<std::endl;fixture.readAllStandardOutput();assert(kill(static_cast<pid_t>(fixture.processId()),sig)==0);
        QElapsedTimer elapsed;elapsed.start();QByteArray output;
        while(!output.contains(marker)){if(elapsed.elapsed()>=10000){std::cerr<<"Fixture output: "<<output.constData()<<" stderr: "<<fixture.readAllStandardError().constData()<<std::endl;assert(false);}fixture.waitForReadyRead(100);output+=fixture.readAllStandardOutput();}
    };
    for(const auto& bad:std::vector<std::pair<QString,QString>>{{"byte","300"},{"small","-200"},{"ratio","abc"},{"enabled","yes"}}){
        input(bad.first)->setText(bad.second);apply->click();
        assert(backend->sets==0 && status->text().startsWith("Invalid input:"));
        assert(current("gain")=="5");revert->click();
    }
    const auto service=backend->real.QueryServices()[0];kcf_tool::DataSnapshot request,response;
    assert(backend->real.GetTypeTemplate(service.identity.runtime,service.request_type_id,request)==0);
    const auto watch=[&](unsigned count){wait([&]{assert(backend->real.CallService(service.identity,request,response)==0);return std::stoul(Field(response,"watches"))>=count;});};
    signal(SIGTTOU,"PARAMETER_UPDATED");watch(1);value_refresh->click();
    assert(current("gain")=="66" && current("enabled")=="false" && current("values[1]")=="14");
    input("gain")->setText("23");input("values[0]")->setText("9");
    const auto list_row=window.findChild<QListWidget*>("parametersList")->currentRow();
    window.findChild<QListWidget*>("parametersList")->setCurrentRow(1);
    assert(window.findChild<QListWidget*>("parametersList")->currentRow()==list_row);
    refresh->click();assert(input("gain")->text()=="23" && backend->sets==0);
    signal(SIGTTOU,"PARAMETER_UPDATED");watch(2);apply->click();
    assert(backend->sets==1 && backend->last_set==0 && !apply->isEnabled());
    assert(current("gain")=="23" && current("enabled")=="true" && current("values[0]")=="9" && current("values[1]")=="24");
    watch(3);
    assert(Field(response,"gain")=="23");
    // A concurrent typed writer after successful Set must be reflected by re-Get.
    after_set=[&]{signal(SIGTTOU,"PARAMETER_UPDATED");};
    input("gain")->setText("31");apply->click();
    assert(backend->sets==2 && current("gain")=="66" && !apply->isEnabled());
    // First Get is GUI's fresh value, second is the real Set client's seed Get.
    // Recreate AFTER that client's Open/Get, immediately before its actual Set.
    after_get=[&]{signal(SIGTTIN,"PARAMETER_RECREATED");};get_countdown=2;
    input("gain")->setText("99");apply->click();
    assert(get_countdown==0 && backend->sets==3 && backend->last_set==-ESTALE);
    assert(input("gain")->text()=="99" && current("gain")=="66" && !apply->isEnabled());
    assert(status->text().contains("Parameter was recreated"));
    assert(backend->real.CallService(service.identity,request,response)==0);
    assert(Field(response,"gain")=="88" && Field(response,"old_gain")=="66");
    revert->click();value_refresh->click();assert(current("gain")=="88");
    fixture.terminate();assert(fixture.waitForFinished(10000)&&fixture.exitCode()==0);
    value_refresh->click();assert(status->text().contains("failed") && !apply->isEnabled());
    refresh->click();assert(window.findChild<QListWidget*>("parametersList")->count()==0);
    window.close();std::cout<<"PASS KT5: validation before Set, arrays, fresh overlay, readback, watcher, same-process stale Set, external update, disappearance\n";
}
