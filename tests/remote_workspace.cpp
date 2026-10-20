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
#include <thread>
using namespace kcf_tool;using namespace std::chrono_literals;
QApplication* application;
template<class F>void Wait(F f){auto end=std::chrono::steady_clock::now()+15s;while(!f()){assert(std::chrono::steady_clock::now()<end);application->processEvents();std::this_thread::sleep_for(10ms);}}
RemoteReply Request(KcfBackend& b,RemoteCommand c){auto f=b.SubmitRemote(c);Wait([&]{return f.wait_for(0ms)==std::future_status::ready;});return f.get();}
RemoteEndpoint Endpoint(KcfBackend& b,const std::string& kind,int host,const std::string& name={}){RemoteEndpoint e;Wait([&]{for(const auto& h:b.GetNetworkSnapshot().hosts)if(h.id.substr(0,2)==(host==1?"01":"02"))for(const auto& v:h.endpoints)if(v.kind==kind&&(name.empty()||v.name==name)&&v.role!=(kind=="T"?"SUBSCRIBER":"CLIENT")){e=v;return true;}return false;});return e;}
void Count(DataSnapshot& v,const std::string& text){for(auto& f:v.fields)if(f.name=="count"){f.value=text;return;}assert(false);}
std::string Count(const DataSnapshot& v){for(const auto& f:v.fields)if(f.name=="count")return f.value;return {};}
int main(int argc,char** argv){QApplication app(argc,argv);application=&app;auto backend=std::make_unique<KcfBackend>();auto* b=backend.get();assert(!b->StartNetwork({"10.203.0.2",37651,"63000000000000000000000000000000"}));MainWindow window(std::move(backend));window.show();std::cout<<"{\"ready\":1}"<<std::endl;
    RemoteEndpoint old;std::string op;
    while(std::getline(std::cin,op)&&op!="QUIT"){
        if(op=="CHECK"){
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
