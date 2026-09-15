#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf_tool/ui/main_window.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <cassert>
#include <iostream>
class ServiceBackend: public kcf_tool::ToolBackend {
public:
    kcf_tool::KcfBackend real;unsigned sets=0;int last_set=0;std::atomic<unsigned> calls{0};
    std::vector<kcf_tool::ServiceInfo> QueryServices()override{return real.QueryServices();}
    int GetTypeTemplate(const kcf_tool::RuntimeIdentity& id,std::uint64_t type,kcf_tool::DataSnapshot& out)override{return real.GetTypeTemplate(id,type,out);}
    int ValidateService(const kcf_tool::EndpointIdentity& id,const kcf_tool::DataSnapshot& request)override{return real.ValidateService(id,request);}
    int CallService(const kcf_tool::EndpointIdentity& id,const kcf_tool::DataSnapshot& request,kcf_tool::DataSnapshot& response,std::uint32_t timeout,std::uint32_t retries)override{++calls;return real.CallService(id,request,response,timeout,retries);}
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

int main(int argc,char** argv){
    QApplication app(argc,argv);assert(argc==2);
    const auto prefix="/kt6_"+QString::number(QCoreApplication::applicationPid());
    QProcess fixture;fixture.start(argv[1],{"--element",prefix,"--services"});assert(fixture.waitForStarted());
    auto owned=std::make_unique<ServiceBackend>();auto* backend=owned.get();
    kcf_tool::MainWindow window(std::move(owned),"KCF");window.show();
    auto* refresh=window.findChild<QPushButton*>("refreshButton");
    auto* services=window.findChild<QListWidget*>("servicesList");
    auto* request=window.findChild<QTableWidget*>("serviceRequest");
    auto* response=window.findChild<QTableWidget*>("serviceResponse");
    auto* call=window.findChild<QPushButton*>("callServiceButton");
    auto* status=window.findChild<QLabel*>("serviceStatus");
    const auto wait=[&](auto condition){QElapsedTimer elapsed;elapsed.start();while(!condition()){assert(elapsed.elapsed()<10000);app.processEvents();QThread::msleep(5);}};
    const auto select=[&](const QString& name){for(int i=0;i<services->count();++i)if(services->item(i)->text().startsWith(prefix+name+" [")){services->setCurrentRow(i);return true;}return false;};
    wait([&]{refresh->click();return services->count()==3 && select("/add") && call->isEnabled();});
    const auto input=[&](const QString& name){for(int i=0;i<request->rowCount();++i)if(request->item(i,0)->text()==name)return qobject_cast<QLineEdit*>(request->cellWidget(i,2));assert(false);return static_cast<QLineEdit*>(nullptr);};
    const auto value=[&](const QString& name){for(int i=0;i<response->rowCount();++i)if(response->item(i,0)->text()==name)return response->item(i,2)->text();assert(false);return QString();};
    assert(request->rowCount()==8 && response->rowCount()==9);
    assert(window.findChild<QLabel*>("serviceMetadata")->text().contains("service_id 10"));
    for(const auto& bad:std::vector<std::pair<QString,QString>>{{"byte","300"},{"small","-200"},{"ratio","abc"}}){
        input(bad.first)->setText(bad.second);call->click();assert(backend->calls==0 && status->text().startsWith("Request rejected:"));input(bad.first)->setText("0");
    }
    // The GUI fixes array shape; malformed model inputs are rejected by the same validation API.
    const auto service=backend->real.QueryServices()[0];kcf_tool::DataSnapshot malformed;
    assert(backend->real.GetTypeTemplate(service.identity.runtime,service.request_type_id,malformed)==0);
    malformed.fields.back().array_values.pop_back();assert(backend->ValidateService(service.identity,malformed)==-EMSGSIZE);
    assert(backend->real.GetTypeTemplate(service.identity.runtime,service.request_type_id,malformed)==0);
    ++malformed.type_id;assert(backend->ValidateService(service.identity,malformed)==-EPROTOTYPE);assert(backend->calls==0);
    input("a")->setText("2");input("b")->setText("3");input("values[2]")->setText("7");
    assert(backend->calls==0);call->click();assert(!call->isEnabled());wait([&]{return call->isEnabled();});
    assert(status->text()=="Call succeeded" && value("sum")=="5" && value("values[2]")=="7" && value("calls")=="1");
    assert(select("/undefined"));assert(!call->isEnabled() && request->rowCount()==0 && response->rowCount()==0);
    assert(status->text()=="Stable service type descriptor unavailable");
    assert(select("/slow"));assert(input("a")->text()=="0" && value("sum").isEmpty());
    const int slow_row=services->currentRow();int pulses=0;QTimer heartbeat;heartbeat.setInterval(10);QObject::connect(&heartbeat,&QTimer::timeout,[&]{++pulses;});heartbeat.start();
    call->click();assert(!call->isEnabled() && !services->isEnabled() && !request->isEnabled());
    for(int i=0;i<10;++i)call->click();services->setCurrentRow(0);assert(services->currentRow()==slow_row);
    wait([&]{return call->isEnabled();});assert(pulses>=10);assert(backend->calls==2);
    assert(status->text().contains("timed out") && status->text().contains("may have executed") && response->rowCount()==0);
    QElapsedTimer quiet;quiet.start();while(quiet.elapsed()<1000){app.processEvents();QThread::msleep(5);}
    assert(backend->calls==2 && call->isEnabled()); // no Tool-level retry after timeout
    assert(select("/add"));input("a")->setText("-2");input("b")->setText("3");call->click();wait([&]{return call->isEnabled();});
    assert(value("sum")=="1" && value("success")=="false" && status->text()=="Call succeeded");
    assert(value("calls")=="3"); // KCF internal retries did not reexecute slow callback
    fixture.terminate();assert(fixture.waitForFinished(10000)&&fixture.exitCode()==0);
    call->click();assert(response->rowCount()==0 && status->text().startsWith("Request rejected:"));
    refresh->click();assert(services->count()==0 && request->rowCount()==0 && response->rowCount()==0 && !call->isEnabled());
    window.close();std::cout<<"PASS KT6: forms, validation before call, async heartbeat, timeout/no retry/double call, arrays, switch, exit\n";
}
