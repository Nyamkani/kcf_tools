#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf/network/control_node.hpp"
#include "kcf/introspection/detail/runtime_registry.hpp"
#include "kcf/ipc/publisher.hpp"
#include "kcf_tool/ui/main_window.hpp"
#include <QApplication>
#include <QSettings>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <cassert>
#include <thread>
#include <iostream>
using namespace kcf_tool;using namespace std::chrono_literals;namespace n=kcf::network;
struct Sample{std::uint64_t value;};
namespace kcf{template<>struct TypeDescriptorTraits<Sample>{static constexpr bool defined=true;static TypeDescriptor Get(){auto d=MakeTypeDescriptor<Sample>("tool.observer.sample");d.field_count=1;d.fields[0]=MakeField<Sample,std::uint64_t>("value",0);return d;}};}
template<class F>void Wait(F f){auto end=std::chrono::steady_clock::now()+12s;while(!f()){assert(std::chrono::steady_clock::now()<end);QApplication::processEvents();std::this_thread::sleep_for(20ms);}}
RemoteReply Request(KcfBackend& b,RemoteCommand c){auto f=b.SubmitRemote(c);Wait([&]{return f.wait_for(0ms)==std::future_status::ready;});return f.get();}
int main(int argc,char** argv){QApplication app(argc,argv);
 kcf::detail::BeginRuntimeIntrospection(kcf::ExecutionMode::STANDALONE);kcf::detail::UpdateRuntimeIntrospection(kcf::ProcessState::RUNNING,0);
 kcf::Publisher<Sample> pub;assert(!pub.Create("/observer/value"));
 n::ControlConfig c;c.discovery.host.bytes[0]=10;c.discovery.boot_id.bytes[0]=42;c.discovery.interface_address="127.0.0.1";c.discovery.port=48651;c.discovery.announce_interval=50ms;
 c.metadata.runtime_pids={getpid()};c.metadata.topic_exposure.enabled=true;c.metadata.topic_exposure.allow_prefixes={"/observer"};c.topic_observer_enabled=true;c.topics.enabled=true;
 assert(c.descriptor_query_clients.empty());n::ControlNode gateway;assert(!gateway.Start(c));
 QSettings settings("KCF","Tools");settings.setValue("network/controlHostId","63000000000000000000000000000000");
 auto ptr=std::make_unique<KcfBackend>();auto* b=ptr.get();assert(!b->StartNetwork({"127.0.0.1",48651}));MainWindow window(std::move(ptr));window.show();
 assert(!b->ConfigureLocalGateway("/tmp/kcf-missing-local-gateway/socket",true));
 Wait([&]{return window.findChild<QLabel*>("localGatewayStatus")->text().contains("ERROR");});
 assert(window.findChild<QLineEdit*>("networkHostId")->text().isEmpty());
 RemoteEndpoint e;Wait([&]{auto s=b->GetNetworkSnapshot();if(!s.network_client_available)return false;assert(!s.remote_control_available&&!s.explicit_control_identity);for(auto& h:s.hosts)for(auto& ep:h.endpoints)if(ep.name=="/observer/value"){e=ep;return true;}return false;});
 const auto identity=b->GetNetworkSnapshot().observer_host;assert(!identity.empty());
 auto* hosts=window.findChild<QComboBox*>("remoteHostSelector");Wait([&]{return hosts->findData(QString::fromStdString(e.identity.host_id))>=0;});hosts->setCurrentIndex(hosts->findData(QString::fromStdString(e.identity.host_id)));
 auto* list=window.findChild<QListWidget*>("remoteEndpoints2");Wait([&]{return list->count()>0;});list->setCurrentRow(0);
 Wait([&]{for(auto* p:window.findChildren<QPushButton*>())if(p->text()=="Subscribe")return p->isEnabled();return false;});
 for(auto* p:window.findChildren<QPushButton*>())if(p->text()=="Set + Readback"||p->text()=="Call once"||p->text()=="Start Goal")assert(!p->isEnabled());
 RemoteCommand command;command.endpoint=e;for(auto op:{RemoteOperation::Prepare,RemoteOperation::Get,RemoteOperation::Set,RemoteOperation::Call,RemoteOperation::Goal,RemoteOperation::Cancel}){command.operation=op;assert(Request(*b,command).error==-EACCES);}
 command.operation=RemoteOperation::Subscribe;auto sub=Request(*b,command);assert(!sub.error&&sub.handle);
 RemoteCommand poll;poll.operation=RemoteOperation::Poll;Wait([&]{assert(!pub.Publish({123}));for(auto& a:Request(*b,poll).activities)if(a.handle==sub.handle&&a.has_value){assert(a.feedback.fields[0].value=="123");return true;}return false;});
 gateway.Stop();assert(!gateway.Start(c));Wait([&]{for(auto& a:Request(*b,poll).activities)if(a.handle==sub.handle)return a.communication_lost||a.terminal;return false;});
 command.operation=RemoteOperation::Unsubscribe;command.handle=sub.handle;assert(!Request(*b,command).error);
 b->StopNetwork();assert(!b->StartNetwork({"127.0.0.1",48651}));Wait([&]{auto s=b->GetNetworkSnapshot();return s.network_client_available&&s.observer_host!=identity;});
 b->StopNetwork();assert(!b->StartNetwork({"127.0.0.1",48651,"63000000000000000000000000000000"}));Wait([&]{auto s=b->GetNetworkSnapshot();return s.remote_control_available&&s.explicit_control_identity;});
 window.close();Wait([&]{return !window.isVisible();});gateway.Stop();pub.Close();pub.Unlink();kcf::detail::EndRuntimeIntrospection(kcf::ProcessState::STOPPED,0);
 std::cout<<"Temporary observer Topic E2E, UI control denial, saved identity, stale session, restart, explicit identity PASS\n";
}
