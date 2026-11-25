#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf_tool/ui/main_window.hpp"
#include <QApplication>
#include <QComboBox>
#include <QListWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <cassert>
#include <iostream>
#include <sstream>
#include <thread>
using namespace kcf_tool;using namespace std::chrono_literals;
QApplication* application;
template<class F>void Wait(F f){auto end=std::chrono::steady_clock::now()+15s;while(!f()){assert(std::chrono::steady_clock::now()<end);application->processEvents();std::this_thread::sleep_for(10ms);}}
RemoteReply Request(KcfBackend& b,RemoteCommand c){auto f=b.SubmitRemote(c);Wait([&]{return f.wait_for(0ms)==std::future_status::ready;});return f.get();}
RemoteEndpoint Endpoint(KcfBackend& b,const std::string& kind,int host,const std::string& name={}){RemoteEndpoint e;Wait([&]{for(const auto& h:b.GetNetworkSnapshot().hosts)if(h.id.substr(0,2)==(host==1?"01":"02"))for(const auto& v:h.endpoints)if(v.kind==kind&&(name.empty()||v.name==name)&&v.role!=(kind=="T"?"SUBSCRIBER":"CLIENT")){e=v;return true;}return false;});return e;}
void Count(DataSnapshot& v,const std::string& text){for(auto& f:v.fields)if(f.name=="count"){f.value=text;return;}assert(false);}
std::string Count(const DataSnapshot& v){for(const auto& f:v.fields)if(f.name=="count")return f.value;return {};}
int main(int argc,char** argv){QApplication app(argc,argv);application=&app;auto backend=std::make_unique<KcfBackend>();auto* b=backend.get();assert(!b->StartNetwork({"10.203.0.2",37651,"63000000000000000000000000000000"}));MainWindow window(std::move(backend));window.show();std::cout<<"{\"ready\":1}"<<std::endl;
    RemoteEndpoint old, retained_topic, retained_parameter;RemoteHost before;
    QString selected_key;std::uint64_t retained_handle=0,received_before=0;
    const auto list=[&](int page){return window.findChild<QListWidget*>(QString("remoteEndpoints%1").arg(page));};
    const auto button=[&](const QString& text){for(auto* v:window.findChildren<QPushButton*>())if(v->text()==text)return v;assert(false);return static_cast<QPushButton*>(nullptr);};
    const auto host=[&]{for(const auto& h:b->GetNetworkSnapshot().hosts)if(h.id==retained_topic.identity.host_id)return h;return RemoteHost{};};
    const auto selected=[&]{return list(2)->currentItem()?list(2)->currentItem()->data(Qt::UserRole).toString():QString{};};
    const auto ready_label=[&]{for(auto* v:window.findChildren<QLabel*>())if(v->text().startsWith("READY\n")&&v->text().contains(QString::fromStdString(retained_topic.id)))return true;return false;};
    const auto activity=[&]{RemoteCommand c;c.operation=RemoteOperation::Poll;return Request(*b,c).activities;};
    std::string op;
    while(std::getline(std::cin,op)&&op!="QUIT"){
        if(op=="SNAPSHOT_SELECT"){
            retained_topic=Endpoint(*b,"T",2,"/virtual/shared/topic");retained_parameter=Endpoint(*b,"P",2);before=host();
            auto* hosts=window.findChild<QComboBox*>("remoteHostSelector");
            Wait([&]{return hosts->findData(QString::fromStdString(before.id))>=0;});hosts->setCurrentIndex(hosts->findData(QString::fromStdString(before.id)));
            Wait([&]{for(int row=0;row<list(2)->count();++row)if(list(2)->item(row)->text().contains(QString::fromStdString(retained_topic.id))){list(2)->setCurrentRow(row);return true;}return false;});
            list(3)->setCurrentRow(0);selected_key=selected();Wait(ready_label);
            assert(before.metadata_ready&&before.last_complete_snapshot.valid&&!before.last_complete_snapshot.stale);
            std::cout<<"{\"ready_selection\":1,\"revision\":\""<<before.revision<<"\",\"session\":\""<<before.session<<"\"}"<<std::endl;
        }else if(op=="SNAPSHOT_SUBSCRIBE"){
            Wait([&]{return button("Subscribe")->isEnabled();});button("Subscribe")->click();
            Wait([&]{for(const auto& a:activity())if(a.topic&&a.endpoint.id==retained_topic.id&&!a.terminal&&a.received_packets){retained_handle=a.handle;received_before=a.received_packets;return true;}return false;});
            // Actual veth Remote Topic -> decoded fields -> Qt Plot/CSV gating.
            auto* remote_field=window.findChild<QComboBox*>("remoteTopicPlotField");
            auto* remote_export=window.findChild<QPushButton*>("remoteTopicPlotExport");
            auto* remote_summary=window.findChild<QLabel*>("remoteTopicPlotSummary");
            auto* remote_status=window.findChild<QLabel*>("remoteStatus2");
            assert(remote_field && remote_export && remote_summary && remote_status);
            Wait([&]{return remote_export->isEnabled() && remote_field->isEnabled() &&
                remote_field->count()>0 && !remote_summary->text().contains("Remote observed=0");});
            assert(remote_field->findText("count")>=0);
            // Verified sender-side diagnostics must refer to the same route,
            // not merely to another Publisher sharing the Topic name.
            Wait([&]{return remote_status->text().contains("Sender route=") &&
                !remote_status->text().contains("Sender route diagnostics: Unavailable");});
            bool matched=false;
            for(const auto& a:activity())if(a.handle==retained_handle && a.topic){
                const auto sample=b->GetNetworkSnapshot();
                const auto now=static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
                const auto* route=FindRemoteTopicSenderRoute(sample,a,now);
                matched=route && route->route_id==a.route_id &&
                    remote_status->text().contains(QString("Sender route=%1").arg(route->route_id));
            }
            assert(matched);
            // Export the actually decoded subscription samples through the same
            // model used by the GUI. CSV content is not synthesized from counters.
            TopicPlotHistory captured;
            captured.Reset(retained_topic.name,retained_topic.value_type.type_id);
            Wait([&]{for(const auto& a:activity())if(a.handle==retained_handle&&a.topic&&a.has_value){
                if(captured.Observe(a.feedback,TopicPlotHistory::Clock::now())==
                   TopicPlotHistory::ObserveResult::RECORDED)
                    return captured.Records().size()>=2;
                }return false;});
            std::ostringstream output;
            assert(captured.WriteCsv(output));
            const auto csv=output.str();
            assert(csv.find("observed_elapsed_ms,transport_sequence,unobserved_sequence_steps")!=std::string::npos);
            assert(csv.find("count")!=std::string::npos);
            assert(csv.find('\n')!=std::string::npos);
            std::cout<<"{\"handle\":"<<retained_handle<<",\"received\":"<<received_before
                     <<",\"remote_plot\":1,\"csv_ready\":1,\"sender_route_verified\":1}"<<std::endl;
        }else if(op=="SNAPSHOT_UPDATING"){
            Wait([&]{auto h=host();return h.revision!=before.revision&&!h.metadata_ready&&list(2)->toolTip().contains("STALE");});
            auto h=host();assert(h.id==before.id&&h.boot_id==before.boot_id&&h.session==before.session&&h.state=="ONLINE");
            assert(h.endpoints.empty()&&h.last_complete_snapshot.valid&&h.last_complete_snapshot.stale&&h.last_complete_snapshot.revision==before.revision);
            assert(selected()==selected_key&&list(2)->currentItem()->text().contains("STALE"));
            assert(!button("Subscribe")->isEnabled()&&!button("Get / Revert")->isEnabled());
            // Bypass disabled widgets to exercise the UI request gate as well.
            QMetaObject::invokeMethod(button("Get / Revert"),"clicked",Qt::DirectConnection);
            assert(window.findChild<QLabel*>("remoteStatus3")->text().contains("Request blocked"));
            RemoteCommand c;c.endpoint=retained_topic;c.operation=RemoteOperation::Subscribe;auto sub=Request(*b,c);assert(sub.error&&!sub.handle);
            c.endpoint=retained_parameter;c.operation=RemoteOperation::Get;assert(Request(*b,c).error);
            bool kept=false;for(const auto& a:activity())if(a.handle==retained_handle)kept=true;assert(kept&&button("Unsubscribe")->isEnabled());
            std::cout<<"{\"stale_selection\":1,\"new_requests_blocked\":1,\"retained_handle\":"<<retained_handle<<"}"<<std::endl;
        }else if(op=="SNAPSHOT_COMPLETE"){
            Wait([&]{auto h=host();return h.metadata_ready&&h.revision!=before.revision&&ready_label();});
            auto h=host();assert(h.boot_id==before.boot_id&&h.session==before.session&&selected()==selected_key);
            assert(h.last_complete_snapshot.valid&&!h.last_complete_snapshot.stale&&h.last_complete_snapshot.revision==h.revision);
            // Measure after completion, so packets received before the revision
            // change cannot satisfy the continued-reception assertion.
            bool kept=false;for(const auto& a:activity())if(a.handle==retained_handle){assert(!a.terminal);received_before=a.received_packets;kept=true;}assert(kept);
            Wait([&]{for(const auto& a:activity())if(a.handle==retained_handle)return !a.terminal&&a.received_packets>received_before;return false;});
            button("Unsubscribe")->click();Wait([&]{return button("Subscribe")->isEnabled();});
            // Unsubscribe keeps the historical CSV export available, but a
            // new session/selection must not silently reuse that plot.
            assert(window.findChild<QPushButton*>("remoteTopicPlotExport")->isEnabled());
            before=h;std::cout<<"{\"atomic_complete\":1,\"selection_retained\":1,\"same_handle_received\":1}"<<std::endl;
        }else if(op=="SNAPSHOT_REPLACED"){
            auto next=Endpoint(*b,"T",2,"/virtual/shared/topic");Wait([&]{next=Endpoint(*b,"T",2,"/virtual/shared/topic");return next.identity.registration_id!=retained_topic.identity.registration_id;});
            assert(next.identity.pid==retained_topic.identity.pid&&next.identity.start_ticks==retained_topic.identity.start_ticks);
            Wait([&]{return selected().isEmpty();});assert(!button("Subscribe")->isEnabled());
            std::cout<<"{\"registration_changed\":1,\"selection_invalidated\":1}"<<std::endl;
        }else if(op=="SNAPSHOT_REMOVED"){
            Wait([&]{auto h=host();if(!h.metadata_ready)return false;for(const auto& e:h.endpoints)if(e.name==retained_topic.name)return false;return selected().isEmpty();});
            assert(!button("Subscribe")->isEnabled());std::cout<<"{\"deleted_selection_invalidated\":1}"<<std::endl;
        }else if(op=="SNAPSHOT_EMPTY"){
            Wait([&]{auto h=host();return h.metadata_ready&&h.endpoints.empty()&&h.last_complete_snapshot.valid&&h.last_complete_snapshot.endpoints.empty()&&list(2)->count()==0&&list(3)->count()==0;});
            assert(selected().isEmpty());std::cout<<"{\"valid_empty\":1}"<<std::endl;
        }else if(op=="SNAPSHOT_LOST"){
            Wait([&]{return host().state=="LOST"&&selected().isEmpty();});assert(!host().last_complete_snapshot.valid&&list(2)->count()==0);
            std::cout<<"{\"lost_selection_invalidated\":1}"<<std::endl;
        }else if(op=="SNAPSHOT_SESSION"){
            Wait([&]{return host().session!=before.session&&host().metadata_ready&&selected().isEmpty();});
            RemoteCommand c;c.operation=RemoteOperation::Subscribe;c.endpoint=retained_topic;assert(Request(*b,c).error==-ESTALE);
            std::cout<<"{\"session_selection_invalidated\":1}"<<std::endl;
        }else if(op=="CHECK"){
            auto p=Endpoint(*b,"P",1);auto service=Endpoint(*b,"S",1);auto action=Endpoint(*b,"A",1);old=p;
            RemoteCommand c;c.endpoint=p;c.operation=RemoteOperation::Get;auto get=Request(*b,c);assert(!get.error&&get.value.fields.size()==3);
            c.operation=RemoteOperation::Set;c.input=get.value;Count(c.input,"123");auto set=Request(*b,c);assert(!set.error&&set.response_received&&!set.execution_unknown&&!set.readback_error&&Count(set.value)=="123");
            Count(c.input,"9999999999999999999999999");auto invalid=Request(*b,c);assert(invalid.error&&!invalid.response_received);c.operation=RemoteOperation::Get;assert(Count(Request(*b,c).value)=="123");
            c.endpoint=action;c.operation=RemoteOperation::Prepare;auto goal=Request(*b,c);assert(!goal.error);Count(goal.value,"7");c.input=goal.value;c.operation=RemoteOperation::Goal;auto started=Request(*b,c);assert(!started.error&&started.response_received&&started.handle);
            c.endpoint=service;c.operation=RemoteOperation::Prepare;auto req=Request(*b,c);assert(!req.error);Count(req.value,"4");c.input=req.value;c.operation=RemoteOperation::Call;auto response=Request(*b,c);assert(!response.error&&response.response_received&&Count(response.value)=="8");
            c.endpoint=p;c.operation=RemoteOperation::Get;assert(!Request(*b,c).error); // Action tracking remains independent.
            RemoteCommand poll;poll.operation=RemoteOperation::Poll;bool feedback=false;Wait([&]{for(const auto& a:Request(*b,poll).activities)if(a.handle==started.handle){feedback|=a.has_value;return a.terminal&&!a.result.fields.empty();}return false;});assert(feedback);
            c.endpoint=action;c.operation=RemoteOperation::Goal;c.input=goal.value;auto cancel_goal=Request(*b,c);assert(!cancel_goal.error&&cancel_goal.handle);
            c.operation=RemoteOperation::Cancel;c.handle=cancel_goal.handle;auto canceled=Request(*b,c);assert(!canceled.error&&canceled.response_received&&canceled.accepted);
            Wait([&]{for(const auto& a:Request(*b,poll).activities)if(a.handle==cancel_goal.handle)return a.terminal&&a.state==5;return false;});
            auto one=Endpoint(*b,"T",1,"/virtual/shared/topic"),two=Endpoint(*b,"T",2,"/virtual/shared/topic");assert(one.id!=two.id&&one.name==two.name);
            c.operation=RemoteOperation::Subscribe;c.endpoint=one;auto sub1=Request(*b,c);assert(!sub1.error);c.endpoint=two;auto sub2=Request(*b,c);assert(!sub2.error&&sub2.handle!=sub1.handle);
            Wait([&]{int count=0;for(const auto& v:Request(*b,poll).activities)if(v.topic&&v.has_value){assert(v.received_packets>0&&v.route_id);auto n=std::stoi(Count(v.feedback));if(v.handle==sub1.handle){assert(n>100000&&n<200000);++count;}if(v.handle==sub2.handle){assert(n>200000);++count;}}return count==2;});
            for(auto id:{sub1.handle,sub2.handle}){c.operation=RemoteOperation::Unsubscribe;c.handle=id;assert(!Request(*b,c).error);}
            // Real generic Parameter editor uses queried packed fields, Set and readback.
            auto* hosts=window.findChild<QComboBox*>("remoteHostSelector");Wait([&]{return hosts->findData(QString::fromStdString(p.identity.host_id))>=0;});hosts->setCurrentIndex(hosts->findData(QString::fromStdString(p.identity.host_id)));
            auto* list=window.findChild<QListWidget*>("remoteEndpoints3");Wait([&]{return list->count()>0;});list->setCurrentRow(0);
            auto* input=window.findChild<QTableWidget*>("remoteInput3");Wait([&]{return input->rowCount()==3;});for(int i=0;i<input->rowCount();++i)if(input->item(i,0)->text()=="count")input->item(i,2)->setText("321");
            QPushButton* apply=nullptr;for(auto* button:window.findChildren<QPushButton*>())if(button->text()=="Set + Readback")apply=button;assert(apply);Wait([&]{return apply->isEnabled();});apply->click();
            auto* status=window.findChild<QLabel*>("remoteStatus3");Wait([&]{return status->text().contains("response_received=1")&&status->text().contains("Readback: OK");});c.endpoint=p;c.operation=RemoteOperation::Get;assert(Count(Request(*b,c).value)=="321");
            const auto button=[&](const QString& text){for(auto* b:window.findChildren<QPushButton*>())if(b->text()==text)return b;assert(false);return static_cast<QPushButton*>(nullptr);};
            const auto edit=[&](int page,const QString& value){auto* list=window.findChild<QListWidget*>(QString("remoteEndpoints%1").arg(page));list->setCurrentRow(0);auto* fields=window.findChild<QTableWidget*>(QString("remoteInput%1").arg(page));Wait([&]{return fields->rowCount()==3;});for(int row=0;row<fields->rowCount();++row)if(fields->item(row,0)->text()=="count")fields->item(row,2)->setText(value);};
            std::cerr<<"GUI service"<<std::endl;edit(5,"6");Wait([&]{return button("Call once")->isEnabled();});button("Call once")->click();auto* service_output=window.findChild<QTableWidget*>("remoteOutput5");Wait([&]{return service_output->rowCount()==3;});assert(service_output->item(0,2)->text()=="12");
            std::cerr<<"GUI action"<<std::endl;edit(7,"8");Wait([&]{return button("Start Goal")->isEnabled();});button("Start Goal")->click();auto* goal_status=window.findChild<QLabel*>("remoteStatus7");Wait([&]{return goal_status->text().contains("response_received=1");});
            auto* activity=window.findChild<QListWidget*>("remoteActivities");Wait([&]{return activity->count()>0&&activity->item(activity->count()-1)->data(Qt::UserRole).toULongLong()>cancel_goal.handle;});activity->setCurrentRow(activity->count()-1);auto* cancel_button=button("Cancel selected Goal (ACK is not terminal)");Wait([&]{return cancel_button->isEnabled();});cancel_button->click();Wait([&]{return goal_status->text().contains("ACK accepted=1");});
            std::cerr<<"GUI topic"<<std::endl;auto* topic_list=window.findChild<QListWidget*>("remoteEndpoints2");for(int row=0;row<topic_list->count();++row)if(topic_list->item(row)->text().startsWith("/virtual/shared/topic")){topic_list->setCurrentRow(row);break;}
            Wait([&]{return button("Subscribe")->isEnabled();});button("Subscribe")->click();auto* topic_output=window.findChild<QTableWidget*>("remoteOutput2");Wait([&]{return topic_output->rowCount()==3;});auto* topic_status=window.findChild<QLabel*>("remoteStatus2");Wait([&]{return topic_status->text().contains("ACTIVE")&&!topic_status->text().contains("Measuring");});assert(!topic_status->text().contains("Receive Hz=0.00"));Wait([&]{return button("Unsubscribe")->isEnabled();});button("Unsubscribe")->click();Wait([&]{return topic_output->rowCount()==0;});
            std::cout<<"{\"control\":1,\"topic_identity\":1,\"gui_readback\":1,\"gui_service_action_topic\":1}"<<std::endl;
        }else if(op=="STALE"){
            Wait([&]{for(const auto& h:b->GetNetworkSnapshot().hosts)if(h.id==old.identity.host_id)return h.session!=old.session&&h.state=="ONLINE";return false;});
            RemoteCommand c;c.endpoint=old;c.operation=RemoteOperation::Get;assert(Request(*b,c).error==-ESTALE);c.endpoint=Endpoint(*b,"P",1);assert(!Request(*b,c).error);
            std::cout<<"{\"stale\":1}"<<std::endl;
        }else if(op=="RECONFIGURE"){
            auto* apply=window.findChild<QPushButton*>("networkApply");assert(apply&&apply->isEnabled());apply->click();Wait([&]{return apply->isEnabled()&&b->GetNetworkSnapshot().remote_control_available;});
            RemoteCommand c;c.endpoint=Endpoint(*b,"P",1);c.operation=RemoteOperation::Get;assert(!Request(*b,c).error);
            std::cout<<"{\"reconfigure\":1}"<<std::endl;
        }else if(op=="TIMEOUT"){
            RemoteCommand c;c.endpoint=Endpoint(*b,"S",1);c.operation=RemoteOperation::Prepare;auto r=Request(*b,c);assert(!r.error);Count(r.value,"99");c.input=r.value;c.operation=RemoteOperation::Call;c.timeout_ms=100;
            auto reply=Request(*b,c);std::cerr<<"Timeout probe: error="<<reply.error<<" execution_unknown="<<reply.execution_unknown<<" response_received="<<reply.response_received<<std::endl;assert(reply.error==-ETIMEDOUT&&reply.execution_unknown);
            // No retry: the next operation is a different read-only Parameter query.
            c.endpoint=Endpoint(*b,"P",2);c.operation=RemoteOperation::Get;c.timeout_ms=1000;assert(!Request(*b,c).error);
            std::cout<<"{\"timeout_unknown\":1,\"response_received\":"<<reply.response_received<<"}"<<std::endl;
        }else if(op=="STOP"){
            RemoteCommand c;c.endpoint=Endpoint(*b,"S",1);c.operation=RemoteOperation::Prepare;auto r=Request(*b,c);assert(!r.error);Count(r.value,"99");c.input=r.value;c.operation=RemoteOperation::Call;c.timeout_ms=100;
            auto f=b->SubmitRemote(c);auto stop=std::async(std::launch::async,[&]{return b->ConfigureNetwork({},false);});Wait([&]{return stop.wait_for(0ms)==std::future_status::ready;});assert(!stop.get());Wait([&]{return f.wait_for(0ms)==std::future_status::ready;});assert(f.get().error);assert(b->GetNetworkSnapshot().hosts.empty());
            std::cout<<"{\"cleanup\":1}"<<std::endl;
        }
    }window.close();Wait([&]{return !window.isVisible();});return 0;
}
