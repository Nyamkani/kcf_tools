#include "kcf_tool/ui/remote_workspace.hpp"
#include "kcf_tool/ui/graph_widget.hpp"
#include <QNetworkInterface>
#include <QHostAddress>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QTabWidget>
#include <QStackedWidget>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSettings>
#include <QDateTime>
#include <QRegularExpression>
#include <algorithm>
#include <cstring>
#include <map>
namespace kcf_tool {
namespace {
QString Text(const std::string& s){return QString::fromStdString(s);}
QString Error(int e){return e?QString::fromLocal8Bit(std::strerror(-e))+QString(" (%1)").arg(e):"OK";}
QString Summary(const RemoteReply& r,RemoteOperation op){
    QString text=QString("%1 | response_received=%2 execution_unknown=%3 cancel_unknown=%4 ACK accepted=%5")
        .arg(Error(r.error)).arg(r.response_received).arg(r.execution_unknown).arg(r.cancel_unknown).arg(r.accepted);
    if(op==RemoteOperation::Set)text+=" | Readback: "+(r.error?QString("not attempted"):Error(r.readback_error));
    return text;
}
QString Key(const RemoteEndpoint& e){return Text(e.id)+":"+QString::number(e.session);}
QLabel* Label(){auto* l=new QLabel;l->setTextFormat(Qt::PlainText);l->setWordWrap(true);return l;}
struct Form {
    QTableWidget* table;DataSnapshot data;std::vector<std::pair<std::size_t,int>> rows;bool dirty{false},rendering{false};
    explicit Form(QWidget* parent):table(new QTableWidget(0,3,parent)){table->setHorizontalHeaderLabels({"Field","Type","Value"});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);}
    void Render(const DataSnapshot& d,bool editable){rendering=true;dirty=false;data=d;rows.clear();table->setRowCount(0);
        for(std::size_t i=0;i<d.fields.size();++i){const auto& f=d.fields[i];const auto add=[&](int j,const std::string& value){int r=table->rowCount();table->insertRow(r);rows.push_back({i,j});
            table->setItem(r,0,new QTableWidgetItem(Text(f.name)+(j<0?QString():QString("[%1]").arg(j))));table->setItem(r,1,new QTableWidgetItem(Text(f.type_name)));
            table->setItem(r,2,new QTableWidgetItem(Text(value)));for(int c=0;c<3;++c)if(c!=2||!editable)table->item(r,c)->setFlags(table->item(r,c)->flags()&~Qt::ItemIsEditable);};
            if(f.array_values.empty())add(-1,f.value);else for(std::size_t j=0;j<f.array_values.size();++j)add(j,f.array_values[j]);
        }rendering=false;
    }
    DataSnapshot Edited()const {auto result=data;for(std::size_t r=0;r<rows.size();++r){auto [i,j]=rows[r];auto text=table->item(r,2)->text().toStdString();if(j<0)result.fields[i].value=text;else result.fields[i].array_values[j]=text;}return result;}
};
}
struct RemoteWorkspace::Impl {
    RemoteWorkspace& owner;ToolBackend& backend;QTabWidget& tabs;GraphWidget& graph;
    QComboBox *hosts,*interfaces;QSpinBox *port,*timeout,*tracking;QLineEdit* host_id;QCheckBox* enabled;QLabel* status;QPushButton* apply;
    QTimer timer;std::future<int> configuration;bool closing{false},closed{false},configuration_shutdown{false};std::uint64_t epoch{0};
    NetworkSnapshot snapshot;QString host_selected,configuration_status;std::map<int,QStackedWidget*> stacks;
    QTreeWidget *applications,*elements;std::map<int,std::vector<RemoteEndpoint>> choices;
    struct Panel {QWidget* page;QListWidget* list;QLabel* metadata;QLabel* status;std::unique_ptr<Form> input,output;std::vector<QPushButton*> buttons;RemoteEndpoint selected;std::uint64_t revision{0},subscription{0};bool busy{false};};
    std::map<int,Panel> panels;
    struct Pending {std::future<RemoteReply> future;RemoteCommand command;int page;std::uint64_t epoch,revision;};std::vector<Pending> pending;
    std::future<RemoteReply> polling;std::uint64_t poll_epoch{0};QListWidget* activity_list;QLabel* activity_status;std::unique_ptr<Form> feedback,result;QPushButton* cancel;std::map<std::uint64_t,RemoteActivity> activities;
    struct TopicRate {std::chrono::steady_clock::time_point at{};std::uint64_t packets{0},route{0};double hz{-1};};
    std::map<std::uint64_t,TopicRate> rates;
    QLabel* local_status;QTreeWidget* local_peers;std::uint64_t local_observed{0};
    int ticks{0};
    Impl(RemoteWorkspace& w,ToolBackend& b,QTabWidget& t,GraphWidget& g):owner(w),backend(b),tabs(t),graph(g){
        auto* layout=new QVBoxLayout(&owner);auto* row=new QHBoxLayout;layout->addLayout(row);
        interfaces=new QComboBox;interfaces->setEditable(true);interfaces->setObjectName("networkInterface");interfaces->addItem("0.0.0.0 (all)","0.0.0.0");
        for(const auto& nic:QNetworkInterface::allInterfaces())if(nic.flags()&QNetworkInterface::IsUp)for(const auto& entry:nic.addressEntries())if(entry.ip().protocol()==QAbstractSocket::IPv4Protocol)interfaces->addItem(nic.humanReadableName()+" / "+entry.ip().toString(),entry.ip().toString());
        auto opts=backend.GetNetworkOptions();int index=interfaces->findData(Text(opts.interface_address));if(index>=0)interfaces->setCurrentIndex(index);else interfaces->setEditText(Text(opts.interface_address));
        port=new QSpinBox;port->setRange(1,65535);port->setValue(opts.port);port->setObjectName("networkPort");
        host_id=new QLineEdit;host_id->setObjectName("networkHostId");host_id->setPlaceholderText("32 hex Host ID; blank = temporary observer");
        host_id->setText(Text(opts.host_id)); // Show the active configuration, not a previous saved identity.
        enabled=new QCheckBox("Network enabled");enabled->setChecked(!backend.GetNetworkSnapshot().observer_host.empty()||backend.GetNetworkSnapshot().status.find("disabled")==std::string::npos);
        apply=new QPushButton("Apply network settings");apply->setObjectName("networkApply");
        row->addWidget(enabled);row->addWidget(interfaces,2);row->addWidget(port);row->addWidget(host_id,2);row->addWidget(apply);
        host_id->setToolTip("Use a unique persistent Host ID granted by the Gateway: descriptor_query_clients, parameter_get_clients, parameter_set_clients, service_call_clients, action_goal_clients, action_cancel_clients. IP is not authorization. Trusted LAN only. Stop/disconnect does not stop hardware or send Action Cancel.");
        local_status=Label();local_status->setObjectName("localGatewayStatus");layout->addWidget(local_status);
        local_peers=new QTreeWidget;local_peers->setObjectName("localGatewayPeers");local_peers->setHeaderLabels({"Local Gateway IPC peers (read-only)","Identity / Session / Revision"});local_peers->setMaximumHeight(140);layout->addWidget(local_peers);
        auto* selection=new QHBoxLayout;layout->addLayout(selection);hosts=new QComboBox;hosts->setObjectName("remoteHostSelector");hosts->addItem("Local","");selection->addWidget(hosts,2);status=Label();status->setObjectName("networkStatus");selection->addWidget(status,3);
        timeout=new QSpinBox;timeout->setRange(1,60000);timeout->setValue(1000);timeout->setSuffix(" ms request");tracking=new QSpinBox;tracking->setRange(1,3600000);tracking->setValue(60000);tracking->setSuffix(" ms Action tracking");selection->addWidget(timeout);selection->addWidget(tracking);
        auto* actions=new QWidget;auto* local_actions=new QVBoxLayout(actions);local_actions->addWidget(new QLabel("Remote Actions: select a Network Host above. Local Action control is not provided."));tabs.addTab(actions,"Actions");
        for(int i:{0,1,2,3,5,7}){
            auto* local=tabs.widget(i);auto title=tabs.tabText(i);tabs.removeTab(i);auto* stack=new QStackedWidget;stack->addWidget(local);auto* remote=new QWidget;auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(remote);stack->addWidget(scroll);tabs.insertTab(i,stack,title);stacks[i]=stack;
            auto* box=new QVBoxLayout(remote);
            if(i<2){auto* tree=new QTreeWidget;tree->setHeaderLabels({i==0?"Application / Runtime / Endpoint":"Runtime / Endpoint","Identity / Role"});box->addWidget(tree);if(i==0)applications=tree;else elements=tree;
                QObject::connect(tree,&QTreeWidget::itemActivated,&owner,[this](QTreeWidgetItem* item,int){const auto id=item->data(0,Qt::UserRole).toString();if(!id.isEmpty())Navigate(id);});continue;}
            Panel p;p.page=remote;p.list=new QListWidget;p.list->setObjectName(QString("remoteEndpoints%1").arg(i));box->addWidget(p.list);p.metadata=Label();box->addWidget(p.metadata);
            if(i!=2)box->addWidget(new QLabel(i==3?"Parameter fields (Get before editing)":i==5?"Service Request":"Action Goal"));
            p.input=std::make_unique<Form>(remote);p.input->table->setObjectName(QString("remoteInput%1").arg(i));box->addWidget(p.input->table);
            auto* buttons=new QHBoxLayout;box->addLayout(buttons);
            auto button=[&](const char* name,RemoteOperation op){auto* btn=new QPushButton(name);p.buttons.push_back(btn);buttons->addWidget(btn);QObject::connect(btn,&QPushButton::clicked,&owner,[this,i,op]{Request(i,op);});};
            if(i==2){button("Subscribe",RemoteOperation::Subscribe);button("Unsubscribe",RemoteOperation::Unsubscribe);p.input->table->hide();}
            if(i==3){button("Get / Revert",RemoteOperation::Get);button("Set + Readback",RemoteOperation::Set);}
            if(i==5){button("Load request fields",RemoteOperation::Prepare);button("Call once",RemoteOperation::Call);}
            if(i==7){button("Load goal fields",RemoteOperation::Prepare);button("Start Goal",RemoteOperation::Goal);}
            p.status=Label();p.status->setObjectName(QString("remoteStatus%1").arg(i));box->addWidget(p.status);p.output=std::make_unique<Form>(remote);p.output->table->setObjectName(QString("remoteOutput%1").arg(i));if(i!=7)box->addWidget(new QLabel(i==2?"Latest received value":i==3?"Readback":"Service Response"));box->addWidget(p.output->table);if(i==7)p.output->table->hide();
            panels.emplace(i,std::move(p));
            QObject::connect(panels[i].list,&QListWidget::currentRowChanged,&owner,[this,i](int row){Select(i,row);});
            QObject::connect(panels[i].input->table,&QTableWidget::itemChanged,&owner,[this,i]{if(!panels[i].input->rendering)panels[i].input->dirty=true;});
        }
        auto* action_layout=qobject_cast<QVBoxLayout*>(panels[7].page->layout());activity_list=new QListWidget;activity_list->setObjectName("remoteActivities");action_layout->addWidget(activity_list);cancel=new QPushButton("Cancel selected Goal (ACK is not terminal)");action_layout->addWidget(cancel);activity_status=Label();action_layout->addWidget(activity_status);
        feedback=std::make_unique<Form>(panels[7].page);result=std::make_unique<Form>(panels[7].page);action_layout->addWidget(new QLabel("Latest Feedback"));action_layout->addWidget(feedback->table);action_layout->addWidget(new QLabel("Terminal Result"));action_layout->addWidget(result->table);
        QObject::connect(activity_list,&QListWidget::currentRowChanged,&owner,[this]{ShowActivity();});
        QObject::connect(cancel,&QPushButton::clicked,&owner,[this]{if(panels[7].busy)return;auto* item=activity_list->currentItem();if(!item)return;auto it=activities.find(item->data(Qt::UserRole).toULongLong());if(it==activities.end()||it->second.communication_lost||it->second.terminal)return;RemoteCommand c;c.operation=RemoteOperation::Cancel;c.endpoint=it->second.endpoint;c.handle=it->first;c.timeout_ms=timeout->value();Submit(7,c);});
        QObject::connect(hosts,&QComboBox::currentIndexChanged,&owner,[this]{if(host_selected==hosts->currentData().toString())return;
            if(Dirty()&&QMessageBox::question(&owner,"Discard Remote edits","Discard unsent Remote field edits and change Host?")!=QMessageBox::Yes){QSignalBlocker block(hosts);hosts->setCurrentIndex(hosts->findData(host_selected));return;}
            host_selected=hosts->currentData().toString();for(auto& p:panels){if(p.second.subscription){RemoteCommand cleanup;cleanup.operation=RemoteOperation::Unsubscribe;cleanup.handle=p.second.subscription;backend.SubmitRemote(cleanup);p.second.subscription=0;}Clear(p.second);}for(auto& stack:stacks)stack.second->setCurrentIndex(host_selected.isEmpty()?0:1);Populate();});
        QObject::connect(apply,&QPushButton::clicked,&owner,[this]{Configure(false);});
        graph.open_remote=[this](const GraphNode& node){auto host=hosts->findData(Text(node.host));if(host<0)return;hosts->setCurrentIndex(host);if(host_selected!=Text(node.host))return;
            if(node.kind==GraphKind::Host){tabs.setCurrentIndex(0);return;}if(node.kind==GraphKind::Element){tabs.setCurrentIndex(1);for(int i=0;i<elements->topLevelItemCount();++i)if(elements->topLevelItem(i)->data(0,Qt::UserRole+1).toString()==Text(node.id.substr(15)))elements->setCurrentItem(elements->topLevelItem(i));return;}
            const std::string prefix="remote-endpoint:";if(node.id.find(prefix)==0)Navigate(Text(node.id.substr(prefix.size())));};
        QObject::connect(&timer,&QTimer::timeout,&owner,[this]{Tick();});timer.start(100);Tick();
    }
    bool Dirty()const {for(const auto& p:panels)if(p.second.input->dirty)return true;return false;}
    void Clear(Panel& p){++p.revision;p.selected={};p.input->Render({},false);p.output->Render({},false);p.metadata->clear();p.status->setText("Select a current Remote endpoint");}
    bool Current(const RemoteEndpoint& e)const {for(const auto& h:snapshot.hosts)if(h.id==e.identity.host_id&&h.boot_id==e.identity.boot_id&&h.session==e.session&&h.state=="ONLINE"&&h.compatibility=="COMPATIBLE"&&h.metadata_ready)for(const auto& v:h.endpoints)if(v.id==e.id&&v.session==e.session)return true;return false;}
    void Populate(){
        const RemoteHost* h=nullptr;for(const auto& v:snapshot.hosts)if(Text(v.id)==host_selected)h=&v;
        applications->clear();elements->clear();std::map<std::string,QTreeWidgetItem*> apps,runtimes,elems;
        if(h)for(const auto& e:h->endpoints){
            auto*& app=apps[e.application_id];if(!app){app=new QTreeWidgetItem(applications,{Text(e.application),Text(e.application_id)});}
            auto*& runtime=runtimes[e.runtime_id];if(!runtime)runtime=new QTreeWidgetItem(app,{Text(e.element),Text(e.runtime_id)});
            auto* item=new QTreeWidgetItem(runtime,{Text(e.name),Text(e.kind+" / "+e.role)});item->setData(0,Qt::UserRole,Text(e.id));
            auto*& element=elems[e.runtime_id];if(!element){element=new QTreeWidgetItem(elements,{Text(e.element),Text(e.runtime_id)});element->setData(0,Qt::UserRole+1,Text(e.runtime_id));}
            auto* endpoint=new QTreeWidgetItem(element,{Text(e.name),Text(e.kind+" / "+e.role)});endpoint->setData(0,Qt::UserRole,Text(e.id));
        }
        for(auto& entry:panels){int i=entry.first;auto& p=entry.second;auto key=Key(p.selected);QSignalBlocker block(p.list);p.list->clear();choices[i].clear();int selected=-1;
            if(h)for(const auto& e:h->endpoints)if(e.kind==(i==2?"T":i==3?"P":i==5?"S":"A")){
                if(Key(e)==key)selected=choices[i].size();choices[i].push_back(e);p.list->addItem(Text(e.name+" ["+e.role+"] "+e.id));}
            p.list->setCurrentRow(selected);
            if(!p.selected.id.empty()&&(!Current(p.selected)||selected<0)){Clear(p);p.status->setText("Selection invalidated: LOST / generation changed. Execution may continue on hardware; reselect explicitly.");}
        }
    }
    void Select(int i,int row){auto& p=panels[i];if(row<0||row>=int(choices[i].size()))return;
        if(p.input->dirty&&QMessageBox::question(&owner,"Discard Remote edits","Discard unsent edits and select another endpoint?")!=QMessageBox::Yes){QSignalBlocker block(p.list);for(std::size_t j=0;j<choices[i].size();++j)if(Key(choices[i][j])==Key(p.selected))p.list->setCurrentRow(j);return;}
        if(p.subscription){RemoteCommand cleanup;cleanup.operation=RemoteOperation::Unsubscribe;cleanup.handle=p.subscription;backend.SubmitRemote(cleanup);p.subscription=0;}
        Clear(p);p.selected=choices[i][row];p.metadata->setText(Text(p.selected.id)+QString("\nSession %1 | ").arg(p.selected.session)+Text(p.selected.scope+"\n"+p.selected.type));
        if(i!=2&&Current(p.selected)&&snapshot.remote_control_available&&p.selected.scope=="REMOTE")Request(i,i==3?RemoteOperation::Get:RemoteOperation::Prepare);
    }
    void Navigate(const QString& id){for(auto& entry:choices)for(std::size_t i=0;i<entry.second.size();++i)if(Text(entry.second[i].id)==id){tabs.setCurrentIndex(entry.first);panels[entry.first].list->setCurrentRow(i);return;}}
    void Request(int page,RemoteOperation op){auto& p=panels[page];if(p.busy)return;
        if((op==RemoteOperation::Get||op==RemoteOperation::Prepare)&&p.input->dirty&&QMessageBox::question(&owner,"Discard Remote edits","Replace unsent fields with a fresh value/template?")!=QMessageBox::Yes)return;
        RemoteCommand c;c.operation=op;c.endpoint=p.selected;c.timeout_ms=timeout->value();c.tracking_ms=tracking->value();c.input=p.input->Edited();c.handle=p.subscription;
        if(op==RemoteOperation::Subscribe&&p.subscription){p.status->setText("Unsubscribe the existing subscription first.");return;}
        Submit(page,c);
    }
    void Submit(int page,const RemoteCommand& c){auto& p=panels[page];p.busy=true;p.status->setText("Request in progress; no automatic retry");pending.push_back({backend.SubmitRemote(c),c,page,epoch,p.revision});}
    void Configure(bool shutdown){if(configuration.valid())return;
        if(!shutdown&&Dirty()&&QMessageBox::question(&owner,"Discard Remote edits","Changing Network settings cancels requests and detaches Actions without stopping hardware. Continue?")!=QMessageBox::Yes)return;
        NetworkOptions opts;const auto text=interfaces->currentText();opts.interface_address=(text==interfaces->itemText(interfaces->currentIndex())?interfaces->currentData().toString():text).toStdString();opts.port=port->value();opts.host_id=host_id->text().trimmed().toLower().toStdString();
        if(!shutdown&&!opts.host_id.empty()&&(!QRegularExpression("^[0-9a-f]{32}$").match(Text(opts.host_id)).hasMatch()||opts.host_id==std::string(32,'0'))){status->setText("Host ID must contain 32 hexadecimal digits and must not be zero");return;}
        if(!shutdown){QSettings settings("KCF","Tools");settings.setValue("network/controlHostId",Text(opts.host_id));}
        ++epoch;for(auto& p:panels){Clear(p.second);p.second.subscription=0;}for(auto& a:activities){a.second.communication_lost=true;a.second.execution_unknown=!a.second.terminal;}
        const bool start=!shutdown&&enabled->isChecked();configuration_shutdown=shutdown;apply->setEnabled(false);configuration=std::async(std::launch::async,[this,opts,start]{return backend.ConfigureNetwork(opts,start);});
    }
    void ShowActivity(){auto* item=activity_list->currentItem();if(!item){cancel->setEnabled(false);return;}auto it=activities.find(item->data(Qt::UserRole).toULongLong());if(it==activities.end())return;auto& a=it->second;if(!Current(a.endpoint))a.communication_lost=true;
        static const char* states[]={"IDLE","ACCEPTED","RUNNING","SUCCEEDED","FAILED","CANCELED"};
        activity_status->setText(QString("Goal ACK=%1 | Cancel ACK=%2 | Terminal=%3 | State=%4 | %5\nexecution_unknown=%6 cancel_unknown=%7 | %8")
            .arg(a.accepted).arg(a.cancel_accepted).arg(a.terminal).arg(a.state>=0&&a.state<6?states[a.state]:"UNKNOWN").arg(Error(a.error)).arg(a.execution_unknown).arg(a.cancel_unknown).arg(a.communication_lost?"LOST/detached: hardware state unknown":"Current session"));
        feedback->Render(a.communication_lost?DataSnapshot{}:a.feedback,false);result->Render(a.communication_lost?DataSnapshot{}:a.result,false);cancel->setEnabled(snapshot.remote_control_available&&!a.terminal&&!a.communication_lost&&!configuration.valid()&&!closing&&!panels[7].busy);
    }
    void LocalTick(){
        const auto s=backend.GetLocalGatewaySnapshot();
        const char* connection=s.connection==LocalGatewayConnection::READY?"CONNECTED":s.connection==LocalGatewayConnection::ERROR?"ERROR":s.connection==LocalGatewayConnection::CONNECTING?"CONNECTING":"DISABLED";
        local_status->setText(QString("Local Gateway IPC: %1 | %2 | error=%3").arg(connection).arg(Text(s.socket_path)).arg(s.error)+
            (s.connection==LocalGatewayConnection::READY?QString("\n%1 | Host %2 | Boot %3 | Session %4 | %5 / %6 | Network error %7").arg(Text(s.state)).arg(Text(s.host)).arg(Text(s.boot)).arg(s.session).arg(Text(s.interface_name)).arg(Text(s.ipv4)).arg(s.last_network_error):QString(" | no current snapshot")));
        if(s.observed_at_unix_ms==local_observed)return;local_observed=s.observed_at_unix_ms;local_peers->clear();
        if(s.connection!=LocalGatewayConnection::READY)return;
        for(const auto& p:s.peers){auto* row=new QTreeWidgetItem(local_peers,{Text(p.address+" | "+p.state),Text(p.host)+QString(" | session %1 | revision %2").arg(p.session).arg(p.metadata_revision)});
            for(const auto& e:p.endpoints)new QTreeWidgetItem(row,{Text(e.name),Text(e.identity.host_id)+QString(" / %1 / %2 / %3").arg(e.identity.pid).arg(e.identity.start_ticks).arg(e.identity.registration_id)});
        }
    }
    void Tick(){
        LocalTick();
        using namespace std::chrono_literals;
        if(configuration.valid()&&configuration.wait_for(0ms)==std::future_status::ready){int code=configuration.get();configuration_status="Network settings: "+Error(code);status->setText(configuration_status);apply->setEnabled(true);if(closing){if(configuration_shutdown){closed=true;owner.window()->close();return;}Configure(true);return;}}
        snapshot=backend.GetNetworkSnapshot();
        if(activity_list->currentItem())ShowActivity();
        if(++ticks%5==0){QSignalBlocker block(hosts);hosts->clear();hosts->addItem("Local","");for(const auto& h:snapshot.hosts)hosts->addItem(Text(h.label+" | "+h.address+" | "+h.id+" | "+h.state+" | "+h.compatibility)+QString(" | session %1").arg(h.session),Text(h.id));int at=hosts->findData(host_selected);if(at<0&&!host_selected.isEmpty()){hosts->addItem("Unavailable Host "+host_selected,host_selected);at=hosts->count()-1;}hosts->setCurrentIndex(std::max(0,at));Populate();}
        const auto online=std::count_if(snapshot.hosts.begin(),snapshot.hosts.end(),[](const RemoteHost& h){return h.state=="ONLINE";});
        status->setText(Text(snapshot.status)+QString(" | Remote Hosts ONLINE %1 / %2").arg(online).arg(snapshot.hosts.size())+"\nTool Host: "+Text(snapshot.observer_host)+(snapshot.remote_control_available?" | explicit Control Client":snapshot.network_client_available?" | temporary Topic Observer":" | Discovery-only / unavailable")+" | snapshot "+QDateTime::currentDateTime().toString(Qt::ISODate));
        QString freshness;for(const auto& h:snapshot.hosts)if(Text(h.id)==host_selected){
            const auto observed=h.diagnostics.observed_at_unix_ms;const auto now=QDateTime::currentMSecsSinceEpoch();
            freshness=QString(" | Session %1 | announce age %2 ms | Diagnostics observation %3 / age %4 ms%5").arg(h.session).arg(h.age_ms).arg(observed).arg(observed?now-qint64(observed):-1).arg(!observed||now<qint64(observed)||now-qint64(observed)>10000?" (Unavailable/stale)":"");}
        status->setText(configuration_status+" "+status->text().replace(" | snapshot "," | Snapshot read ")+freshness);
        for(auto it=pending.begin();it!=pending.end();){if(it->future.wait_for(0ms)!=std::future_status::ready){++it;continue;}auto r=it->future.get();auto& p=panels[it->page];p.busy=false;
            if(it->epoch==epoch&&it->command.operation==RemoteOperation::Goal&&r.handle){RemoteActivity a;a.handle=r.handle;a.endpoint=it->command.endpoint;a.accepted=r.response_received&&!r.error;a.execution_unknown=r.execution_unknown;activities[r.handle]=a;}
            if(it->epoch==epoch&&it->revision!=p.revision&&it->command.operation==RemoteOperation::Subscribe&&r.handle){RemoteCommand cleanup;cleanup.operation=RemoteOperation::Unsubscribe;cleanup.handle=r.handle;backend.SubmitRemote(cleanup);}
            if(it->epoch==epoch&&it->revision==p.revision&&(it->command.operation==RemoteOperation::Unsubscribe||Current(it->command.endpoint))){p.status->setText(Summary(r,it->command.operation));if(!r.error){auto op=it->command.operation;
                if(op==RemoteOperation::Prepare||op==RemoteOperation::Get)p.input->Render(r.value,true);
                if(op==RemoteOperation::Set){p.input->dirty=false;if(!r.readback_error){p.input->Render(r.value,true);p.output->Render(r.value,false);}else {p.input->Render({},false);p.output->Render({},false);}}
                if(op==RemoteOperation::Call){p.output->Render(r.value,false);p.input->dirty=false;}
                if(op==RemoteOperation::Goal)p.input->dirty=false;
                if(op==RemoteOperation::Subscribe)p.subscription=r.handle;
                if(op==RemoteOperation::Unsubscribe){p.subscription=0;p.output->Render({},false);}
            }}else if(it->command.operation==RemoteOperation::Set||it->command.operation==RemoteOperation::Call||it->command.operation==RemoteOperation::Goal)p.status->setText("Stale selection result (not applied to new fields): "+Summary(r,it->command.operation));
            it=pending.erase(it);
        }
        if(polling.valid()&&polling.wait_for(0ms)==std::future_status::ready){auto r=polling.get();if(poll_epoch==epoch&&!configuration.valid()&&!closing)for(auto a:r.activities){if(a.topic){auto& p=panels[2];if(p.subscription!=a.handle)continue;double hz=-1;
                    if(a.state==1&&!a.communication_lost){auto& rate=rates[a.handle];
                        if(rate.at==std::chrono::steady_clock::time_point{}||rate.route!=a.route_id||a.received_packets<rate.packets)rate={a.observed_at,a.received_packets,a.route_id,-1};
                        const double dt=std::chrono::duration<double>(a.observed_at-rate.at).count();
                        if(dt>=0.5){rate.hz=(a.received_packets-rate.packets)/dt;rate.at=a.observed_at;rate.packets=a.received_packets;}hz=rate.hz;
                    }else rates.erase(a.handle);
                    if(a.has_value)p.output->Render(a.feedback,false);else if(a.communication_lost||a.terminal){p.output->Render({},false);rates.erase(a.handle);}
                    static const char* topic_states[]={"WAITING_ROUTE","ACTIVE","LOST","STALE","ERROR","CLOSED"};
                    p.status->setText(QString("Topic state=%1 | Sequence=%2 | Receive Hz=%3 | %4%5").arg(a.state>=0&&a.state<6?topic_states[a.state]:"UNKNOWN").arg(a.sequence).arg(a.communication_lost||a.terminal?"Unavailable":hz<0?"Measuring":QString::number(hz,'f',2)).arg(Error(a.error)).arg(a.communication_lost?" | LOST/stale; retained handle must be unsubscribed":""));
                }else activities[a.handle]=std::move(a);}
            for(auto it=activities.begin();activities.size()>128&&it!=activities.end();)if(it->second.terminal||it->second.communication_lost)it=activities.erase(it);else ++it;
            QSignalBlocker block(activity_list);auto selected=activity_list->currentItem()?activity_list->currentItem()->data(Qt::UserRole).toULongLong():0;activity_list->clear();for(auto& a:activities){auto* item=new QListWidgetItem(Text(a.second.endpoint.name+" | "+a.second.endpoint.identity.host_id)+QString(" | Goal handle %1").arg(a.first),activity_list);item->setData(Qt::UserRole,qulonglong(a.first));if(a.first==selected)activity_list->setCurrentItem(item);}ShowActivity();
        }
        if(ticks%5==0&&!polling.valid()&&snapshot.network_client_available&&!configuration.valid()&&!closing){RemoteCommand c;c.operation=RemoteOperation::Poll;poll_epoch=epoch;polling=backend.SubmitRemote(c);}
        for(auto& entry:panels){auto& p=entry.second;bool live=Current(p.selected)&&p.selected.scope=="REMOTE"&&(entry.first==2?snapshot.network_client_available:snapshot.remote_control_available)&&!p.busy&&!configuration.valid()&&!closing;
            bool role=p.selected.role==(entry.first==2?"PUBLISHER":entry.first==3?"OWNER":"SERVER");for(auto* button:p.buttons)button->setEnabled(live&&role);
            if(entry.first==2){p.buttons[0]->setEnabled(live&&role&&!p.subscription);p.buttons[1]->setEnabled(p.subscription&&!p.busy&&!configuration.valid());}
            if(entry.first!=2&&p.buttons.size()>1)p.buttons[1]->setEnabled(live&&role&&!p.input->data.fields.empty());
        }
    }
};
RemoteWorkspace::RemoteWorkspace(ToolBackend& b,QTabWidget& t,GraphWidget& g,QWidget* p):QWidget(p),impl_(std::make_unique<Impl>(*this,b,t,g)){}
RemoteWorkspace::~RemoteWorkspace(){impl_->timer.stop();if(impl_->configuration.valid())impl_->configuration.wait();}
bool RemoteWorkspace::BeginClose(){if(impl_->closed)return true;if(!impl_->configuration.valid()&&impl_->pending.empty()&&impl_->snapshot.status.find("disabled / unavailable")!=std::string::npos)return true;if(impl_->Dirty()&&QMessageBox::question(this,"Discard Remote edits","Discard unsent Remote edits and close? Active hardware is not stopped.")!=QMessageBox::Yes)return false;impl_->closing=true;impl_->Configure(true);return false;}
}
