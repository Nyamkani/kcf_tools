#include "kcf_tool/ui/main_window.hpp"
#include <QFormLayout>
#include <QTreeWidget>
#include <QCloseEvent>
#include <QHeaderView>
#include <QTableWidget>
#include <QTimer>
#include <QThread>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace kcf_tool {
namespace {
QString Text(const std::string& value) { return QString::fromStdString(value); }
QString Number(std::uint64_t value) { return QString::number(static_cast<qulonglong>(value)); }
QString ErrorText(int result) {
    if (result == -ENOTSUP) return "Not supported";
    if (result == -ENOENT) return "Not found";
    if (result == -EPERM) return "Read-only / Permission denied";
    if (result < 0) return QString::fromLocal8Bit(std::strerror(-result));
    return QString("Unexpected backend result: %1").arg(result);
}
QString Names(const std::vector<std::string>& names) {
    QStringList values;
    for (const auto& name : names) values.append(Text(name));
    return values.isEmpty() ? QStringLiteral("None") : values.join(", ");
}
// Shared scalar/array row traversal and text extraction for Parameter/Service forms.
template<class F> void FieldRows(const DataSnapshot& value,F add){
    for(std::size_t i=0;i<value.fields.size();++i){const auto& field=value.fields[i];
        if(field.array_values.empty())add(i,-1,field.value);
        else for(std::size_t j=0;j<field.array_values.size();++j)add(i,static_cast<int>(j),field.array_values[j]);
    }
}
DataSnapshot EditedFields(DataSnapshot value,const std::vector<QLineEdit*>& inputs,
                          const std::vector<std::pair<std::size_t,int>>& rows){
    for(std::size_t row=0;row<inputs.size();++row){auto [i,j]=rows[row];const auto text=inputs[row]->text().toStdString();
        if(j<0)value.fields[i].value=text;else value.fields[i].array_values[j]=text;
    }return value;
}
void ResponseRows(QTableWidget* table,const DataSnapshot& value,bool show_values){
    table->setRowCount(0);
    FieldRows(value,[&](std::size_t i,int j,const std::string& text){const auto& field=value.fields[i];
        const int row=table->rowCount();table->insertRow(row);
        table->setItem(row,0,new QTableWidgetItem(Text(field.name)+(j<0?QString():"["+QString::number(j)+"]")));
        table->setItem(row,1,new QTableWidgetItem(Text(field.type_name)));
        table->setItem(row,2,new QTableWidgetItem(show_values?Text(text):QString()));
    });
}
template<std::size_t N>
QFormLayout* Fields(QWidget* parent, const std::array<const char*, N>& titles,
                    std::array<QLabel*, N>& labels) {
    auto* form = new QFormLayout(parent);
    for (std::size_t i = 0; i < N; ++i) {
        labels[i] = new QLabel;
        labels[i]->setTextFormat(Qt::PlainText);
        labels[i]->setTextInteractionFlags(Qt::TextSelectableByMouse);
        labels[i]->setWordWrap(true);
        form->addRow(titles[i], labels[i]);
    }
    return form;
}
QWidget* DetailTab(QTabWidget* tabs, const QString& title, QListWidget*& list,
                   QVBoxLayout*& details) {
    auto* page = new QWidget;
    auto* layout = new QHBoxLayout(page);
    list = new QListWidget;
    list->setObjectName(title.toLower() + "List");
    list->setMinimumWidth(330);
    layout->addWidget(list, 1);
    auto* right = new QWidget;
    details = new QVBoxLayout(right);
    layout->addWidget(right, 2);
    tabs->addTab(page, title);
    return right;
}
template<class T>
void UpdateList(QListWidget* list, const std::vector<T>& items) {
    const QString selected = list->currentItem() ? list->currentItem()->text() : QString();
    const QSignalBlocker blocker(list);
    list->clear();
    int row = items.empty() ? -1 : 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        list->addItem(Text(items[i].name));
        if (Text(items[i].name) == selected) row = static_cast<int>(i);
    }
    list->setCurrentRow(row);
}
template<std::size_t N>
void Clear(std::array<QLabel*, N>& fields) {
    for (auto* field : fields) field->clear();
}
}

MainWindow::MainWindow(std::unique_ptr<ToolBackend> backend,
                       const QString& backend_name, QWidget* parent)
    : QMainWindow(parent), backend_(std::move(backend)) {
    if (!backend_) throw std::invalid_argument("MainWindow requires a backend");
    setWindowTitle("KCF Tool");
    auto* central = new QWidget;
    auto* layout = new QVBoxLayout(central);
    auto* heading = new QHBoxLayout;
    heading->addWidget(new QLabel("KCF Tool"));
    heading->addStretch();
    application_status_ = new QLabel;
    application_status_->setTextFormat(Qt::PlainText);
    heading->addWidget(application_status_);
    layout->addLayout(heading);
    auto* tabs = new QTabWidget;tabs_=tabs;
    layout->addWidget(tabs);
    setCentralWidget(central);

    auto* application = new QWidget;
    auto* app_layout = new QVBoxLayout(application);
    auto* app_form = new QWidget;
    Fields<7>(app_form, {"Application Name", "Application State", "Total Elements",
                        "Running", "Error", "Topic Count", "Parameter Count"}, application_fields_);
    app_layout->addWidget(app_form);
    auto* refresh = new QPushButton("Refresh");
    refresh->setObjectName("refreshButton");
    app_layout->addWidget(refresh, 0, Qt::AlignLeft);
    application_tree_=new QTreeWidget;
    application_tree_->setObjectName("applicationsTree");
    application_tree_->setHeaderLabels({"Application / Element", "PID", "Start ticks", "State / Membership"});
    app_layout->addWidget(application_tree_);
    connect(application_tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item){
        if(service_busy_)return;
        const auto index=item?item->data(0,Qt::UserRole):QVariant();
        element_list_->setCurrentRow(index.isValid()?index.toInt():-1);
    });
    connect(application_tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item){
        if(item && item->data(0,Qt::UserRole).isValid())tabs_->setCurrentIndex(1);
    });
    tabs->addTab(application, "Applications");
    connect(refresh, &QPushButton::clicked, this, &MainWindow::Refresh);

    QVBoxLayout* details = nullptr;
    DetailTab(tabs, "Elements", element_list_, details);
    auto* element_form = new QWidget;
    Fields<7>(element_form, {"Name", "PID", "Executable", "Mode", "State", "Heartbeat",
                            "Runtime Error"}, element_fields_);
    element_fields_[5]->setObjectName("elementHeartbeat");
    details->addWidget(element_form);
    element_identity_=new QLabel;
    element_identity_->setObjectName("elementIdentity");
    element_identity_->setTextFormat(Qt::PlainText);
    element_identity_->setWordWrap(true);
    details->addWidget(element_identity_);
    element_endpoints_=new QTreeWidget;
    element_endpoints_->setObjectName("elementEndpoints");
    element_endpoints_->setHeaderLabels({"Endpoint (activate to open)", "Role", "Registration ID"});
    details->addWidget(element_endpoints_);
    connect(element_endpoints_, &QTreeWidget::itemActivated, this, &MainWindow::OpenElementEndpoint);
    details->addStretch();
    connect(element_list_, &QListWidget::currentRowChanged, this, &MainWindow::ShowElement);

    DetailTab(tabs, "Topics", topic_list_, details);
    auto* topic_form = new QWidget;
    Fields<7>(topic_form, {"Name", "Type", "Payload Size", "Frequency", "Sequence",
                          "Publishers", "Subscribers"}, topic_fields_);
    topic_fields_[3]->setObjectName("topicFrequency");
    topic_fields_[4]->setObjectName("topicSequence");
    details->addWidget(topic_form);
    topic_identity_ = new QLabel;
    topic_identity_->setObjectName("topicIdentity");
    topic_identity_->setTextFormat(Qt::PlainText);
    topic_identity_->setWordWrap(true);
    details->addWidget(topic_identity_);
    auto* echo_controls = new QHBoxLayout;
    start_echo_ = new QPushButton("Start Echo");
    start_echo_->setObjectName("startEchoButton");
    stop_echo_ = new QPushButton("Stop Echo");
    stop_echo_->setObjectName("stopEchoButton");
    echo_controls->addWidget(start_echo_);
    echo_controls->addWidget(stop_echo_);
    echo_controls->addStretch();
    details->addLayout(echo_controls);
    echo_status_ = new QLabel("Echo stopped");
    echo_status_->setObjectName("echoStatus");
    echo_status_->setTextFormat(Qt::PlainText);
    details->addWidget(echo_status_);
    echo_table_ = new QTableWidget(0, 3);
    echo_table_->setObjectName("echoFields");
    echo_table_->setHorizontalHeaderLabels({"Field Name", "Type", "Value"});
    echo_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    echo_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    details->addWidget(echo_table_);
    echo_timer_ = new QTimer(this);
    echo_timer_->setObjectName("echoTimer");
    echo_timer_->setInterval(100);
    connect(echo_timer_, &QTimer::timeout, this, &MainWindow::PollEcho);
    connect(start_echo_, &QPushButton::clicked, this, &MainWindow::StartEcho);
    connect(stop_echo_, &QPushButton::clicked, this, &MainWindow::StopEcho);
    connect(topic_list_, &QListWidget::currentRowChanged, this, &MainWindow::ShowTopic);

    DetailTab(tabs, "Parameters", parameter_list_, details);
    auto* parameter_form = new QWidget;
    Fields<4>(parameter_form, {"Name", "Type", "Current Value", "Writable"}, parameter_fields_);
    parameter_fields_[2]->setObjectName("parameterCurrentValue");
    details->addWidget(parameter_form);
    parameter_metadata_ = new QLabel;
    parameter_metadata_->setObjectName("parameterMetadata");
    parameter_metadata_->setTextFormat(Qt::PlainText);
    parameter_metadata_->setWordWrap(true);
    details->addWidget(parameter_metadata_);
    parameter_table_ = new QTableWidget(0,4);
    parameter_table_->setObjectName("parameterFields");
    parameter_table_->setHorizontalHeaderLabels({"Field", "Type", "Current", "Edit"});
    parameter_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    parameter_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    details->addWidget(parameter_table_);
    parameter_editor_ = new QLineEdit;
    parameter_editor_->setObjectName("parameterEditor");
    parameter_editor_->setAccessibleName("New parameter value");
    details->addWidget(parameter_editor_);
    apply_button_ = new QPushButton("Apply");
    details->addWidget(apply_button_, 0, Qt::AlignLeft);
    parameter_refresh_ = new QPushButton("Refresh Value");
    parameter_refresh_->setObjectName("refreshParameterButton");
    parameter_revert_ = new QPushButton("Revert");
    parameter_revert_->setObjectName("revertParameterButton");
    details->addWidget(parameter_refresh_,0,Qt::AlignLeft);
    details->addWidget(parameter_revert_,0,Qt::AlignLeft);
    parameter_status_ = new QLabel;
    parameter_status_->setObjectName("parameterStatus");
    parameter_status_->setTextFormat(Qt::PlainText);
    details->addWidget(parameter_status_);
    connect(parameter_refresh_, &QPushButton::clicked, this, &MainWindow::RefreshParameterValue);
    connect(parameter_revert_, &QPushButton::clicked, this, &MainWindow::RevertParameter);
    connect(parameter_editor_, &QLineEdit::textChanged, this, &MainWindow::ParameterEdited);
    connect(parameter_list_, &QListWidget::currentRowChanged, this, &MainWindow::ShowParameter);
    connect(apply_button_, &QPushButton::clicked, this, &MainWindow::ApplyParameter);
    connect(parameter_editor_, &QLineEdit::returnPressed, this, &MainWindow::ApplyParameter);

    auto* launch = new QWidget;
    auto* launch_layout = new QVBoxLayout(launch);
    launch_layout->addWidget(new QLabel("Application Config"));
    for (const auto* title : {"Load Config", "Save Config"}) {
        auto* button = new QPushButton(title);
        button->setEnabled(false);
        launch_layout->addWidget(button, 0, Qt::AlignLeft);
    }
    launch_layout->addWidget(new QLabel("Element configuration is unavailable."));
    for (const auto* title : {"Start Application", "Stop Application"}) {
        auto* button = new QPushButton(title);
        button->setEnabled(false);
        launch_layout->addWidget(button, 0, Qt::AlignLeft);
    }
    launch_layout->addWidget(new QLabel("Launch/config backend is not implemented in KT-0."));
    launch_layout->addStretch();
    tabs->addTab(launch, "Launch");
    DetailTab(tabs, "Services", service_list_, details);
    service_metadata_=new QLabel;service_metadata_->setObjectName("serviceMetadata");
    service_metadata_->setTextFormat(Qt::PlainText);service_metadata_->setWordWrap(true);details->addWidget(service_metadata_);
    details->addWidget(new QLabel("Request"));
    service_request_=new QTableWidget(0,3);service_request_->setObjectName("serviceRequest");
    service_request_->setHorizontalHeaderLabels({"Field Name","Type","Input"});
    service_request_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    service_request_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);details->addWidget(service_request_);
    service_call_=new QPushButton("Call");service_call_->setObjectName("callServiceButton");details->addWidget(service_call_);
    details->addWidget(new QLabel("Response"));
    service_response_=new QTableWidget(0,3);service_response_->setObjectName("serviceResponse");
    service_response_->setHorizontalHeaderLabels({"Field Name","Type","Value"});
    service_response_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    service_response_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);details->addWidget(service_response_);
    service_status_=new QLabel;service_status_->setObjectName("serviceStatus");service_status_->setTextFormat(Qt::PlainText);
    service_status_->setWordWrap(true);details->addWidget(service_status_);
    connect(service_list_,&QListWidget::currentRowChanged,this,&MainWindow::ShowService);
    connect(service_call_,&QPushButton::clicked,this,&MainWindow::CallService);
    auto* backend_label = new QLabel("Backend: " + backend_name);
    backend_label->setObjectName("backendStatus");
    backend_label->setTextFormat(Qt::PlainText);
    statusBar()->addPermanentWidget(backend_label);
    connection_status_ = new QLabel;
    connection_status_->setObjectName("connectionStatus");
    statusBar()->addPermanentWidget(connection_status_);
    ShowParameter();
    Refresh();
}

MainWindow::~MainWindow() { if(service_thread_)service_thread_->wait();StopEcho(); }
void MainWindow::closeEvent(QCloseEvent* event) {
    if(service_busy_){statusBar()->showMessage("Service call in progress. Close after it finishes.");event->ignore();return;}
    if(parameter_dirty_){statusBar()->showMessage("Unsaved parameter edits: Apply or Revert before closing.");event->ignore();return;}
    StopEcho();
    QMainWindow::closeEvent(event);
}

void MainWindow::Refresh() {
    if(service_busy_)return;
    if(parameter_dirty_){statusBar()->showMessage("Unsaved parameter edits: Apply or Revert before Refresh.");return;}
    const int old_element_row=element_list_->currentRow();
    const bool had_element=old_element_row>=0 && static_cast<std::size_t>(old_element_row)<elements_.size();
    const auto old_element=had_element?elements_[old_element_row].identity:RuntimeIdentity{};
    const bool had_parameter=parameter_row_>=0 && static_cast<std::size_t>(parameter_row_)<parameters_.size();
    const auto previous_parameter=had_parameter?parameters_[parameter_row_].identity:EndpointIdentity{};
    parameter_row_=-1;
    StopEcho();
    echo_requires_refresh_ = false;
    const int previous_row = topic_list_->currentRow();
    const bool had_topic = previous_row >= 0 && static_cast<std::size_t>(previous_row) < topics_.size();
    const auto previous_identity = had_topic ? topics_[previous_row].identity : EndpointIdentity{};
    const int result = backend_->Refresh();
    const auto connection = backend_->GetConnectionState();
    switch (connection) {
    case BackendConnectionState::CONNECTED:
        connection_status_->setText("Connection: Connected"); break;
    case BackendConnectionState::DISCONNECTED:
        connection_status_->setText("Connection: Disconnected"); break;
    case BackendConnectionState::ERROR:
        connection_status_->setText("Connection: Error"); break;
    }
    if (result != 0 || connection != BackendConnectionState::CONNECTED) {
        // Discard stale data when refresh fails or the backend is unavailable.
        applications_.clear();
        elements_.clear();
        topics_.clear();
        parameters_.clear();
        Clear(application_fields_);
        application_status_->setText(connection_status_->text());
        UpdateList(element_list_, elements_);
        UpdateList(topic_list_, topics_);
        UpdateList(parameter_list_, parameters_);
        ShowElement();
        ShowTopic();
        ShowParameter();
        UpdateServices();
        UpdateExplorer();
        discovery_refreshed_=true;
        statusBar()->showMessage("Last refresh: " +
            (result != 0 ? ErrorText(result) : QStringLiteral("Backend unavailable")));
        return;
    }
    const auto application = backend_->GetApplicationInfo();
    applications_ = backend_->GetApplications();
    elements_ = backend_->GetElements();
    topics_ = backend_->GetTopics();
    parameters_ = backend_->GetParameters();
    std::stable_sort(parameters_.begin(),parameters_.end(),[](const auto& a,const auto& b){return a.role=="OWNER" && b.role!="OWNER";});
    application_status_->setText(Text(application.name) + "  " + Text(application.state));
    application_fields_[0]->setText(Text(application.name));
    application_fields_[1]->setText(Text(application.state));
    application_fields_[2]->setText(Number(elements_.size()));
    application_fields_[3]->setText(Number(std::count_if(elements_.begin(), elements_.end(),
        [](const auto& e) { return e.state == "RUNNING"; })));
    application_fields_[4]->setText(Number(std::count_if(elements_.begin(), elements_.end(),
        [](const auto& e) { return e.state == "ERROR" || e.runtime_error != 0; })));
    application_fields_[5]->setText(Number(topics_.size()));
    application_fields_[6]->setText(Number(parameters_.size()));
    UpdateList(element_list_, elements_);
    {
        const QSignalBlocker blocker(element_list_);
        int selected=-1;
        for(std::size_t i=0;i<elements_.size();++i)
            if(had_element && elements_[i].identity==old_element)selected=static_cast<int>(i);
        element_list_->setCurrentRow(selected);
    }
    UpdateList(topic_list_, topics_);
    {
        const QSignalBlocker blocker(topic_list_);
        int selected = had_topic ? -1 : (discovery_refreshed_ || topics_.empty() ? -1 : 0);
        for (std::size_t i = 0; i < topics_.size(); ++i) {
            const auto& t = topics_[i];
            topic_list_->item(static_cast<int>(i))->setText(Text(t.name) + " [" + Text(t.role) +
                " PID " + QString::number(t.identity.runtime.pid) + "]");
            if (had_topic && t.identity == previous_identity) selected = static_cast<int>(i);
        }
        topic_list_->setCurrentRow(selected);
    }
    UpdateList(parameter_list_, parameters_);
    {
        const QSignalBlocker blocker(parameter_list_);
        int selected=had_parameter?-1:(discovery_refreshed_ || parameters_.empty()?-1:0);
        for(std::size_t i=0;i<parameters_.size();++i){const auto& p=parameters_[i];
            parameter_list_->item(static_cast<int>(i))->setText(Text(p.name)+" ["+Text(p.role)+" PID "+QString::number(p.identity.runtime.pid)+"]");
            if(had_parameter && p.identity==previous_parameter)selected=static_cast<int>(i);
        }
        parameter_list_->setCurrentRow(selected);
    }
    ShowElement();
    ShowTopic();
    ShowParameter();
    UpdateServices();
    UpdateExplorer();
    ShowElement();
    discovery_refreshed_=true;
    statusBar()->showMessage("Last refresh: OK");
}

void MainWindow::ShowElement() {
    Clear(element_fields_);
    element_identity_->clear();
    element_endpoints_->clear();
    const int row = element_list_->currentRow();
    if (row < 0 || static_cast<std::size_t>(row) >= elements_.size()) return;
    const auto& e = elements_[row];
    const std::array<QString, 7> values{Text(e.name), QString::number(e.pid), Text(e.executable),
        Text(e.mode), Text(e.state), e.heartbeat_available?Number(e.heartbeat):QStringLiteral("N/A"), QString::number(e.runtime_error)};
    for (std::size_t i = 0; i < values.size(); ++i) element_fields_[i]->setText(values[i]);
    element_identity_->setText("Runtime: PID "+QString::number(e.identity.pid)+" / start_ticks "+Number(e.identity.process_start_ticks)+
        "\nSupervisor: PID "+QString::number(e.supervisor.pid)+" / start_ticks "+Number(e.supervisor.process_start_ticks)+"\nApplication: "+Text(e.application_name));
    const auto add=[&](const char* title,const auto& endpoints,int tab){
        auto* group=new QTreeWidgetItem(element_endpoints_,{title});
        for(std::size_t i=0;i<endpoints.size();++i){const auto& endpoint=endpoints[i];
            if(!(endpoint.identity.runtime==e.identity))continue;
            auto* item=new QTreeWidgetItem(group,{Text(endpoint.name),QString(),Number(endpoint.identity.registration_id)});
            item->setData(0,Qt::UserRole,tab);item->setData(0,Qt::UserRole+1,static_cast<int>(i));
        }
    };
    add("Topics",topics_,2);add("Parameters",parameters_,3);add("Services",services_,5);
    for(int group=0;group<2;++group){auto* parent=element_endpoints_->topLevelItem(group);
        for(int i=0;i<parent->childCount();++i){auto* item=parent->child(i);int index=item->data(0,Qt::UserRole+1).toInt();
            item->setText(1,Text(group==0?topics_[index].role:parameters_[index].role));}}
    element_endpoints_->expandAll();

}
void MainWindow::UpdateExplorer() {
    const QSignalBlocker blocker(application_tree_);
    application_tree_->clear();
    std::vector<std::pair<RuntimeIdentity,QTreeWidgetItem*>> groups;
    for(const auto& app:applications_){
            auto* item=new QTreeWidgetItem(application_tree_,{Text(app.name),QString::number(app.supervisor.pid),Number(app.supervisor.process_start_ticks),
                Text(app.state)+" / managed "+Number(app.managed_element_count)+" / revision "+Number(app.revision)});
            groups.push_back({app.supervisor,item});
        }
    auto* standalone=new QTreeWidgetItem(application_tree_,{"Standalone"});
    for(std::size_t i=0;i<elements_.size();++i){const auto& e=elements_[i];auto* parent=standalone;
        for(const auto& group:groups)if(e.supervisor.pid>0 && e.supervisor==group.first){parent=group.second;break;}
        auto* item=new QTreeWidgetItem(parent,{Text(e.name),QString::number(e.identity.pid),Number(e.identity.process_start_ticks),Text(e.state)});
        item->setData(0,Qt::UserRole,static_cast<int>(i));
        if(element_list_->currentRow()==static_cast<int>(i))application_tree_->setCurrentItem(item);
    }
    application_tree_->expandAll();
}
void MainWindow::OpenElementEndpoint(QTreeWidgetItem* item) {
    if(service_busy_ || !item || !item->data(0,Qt::UserRole).isValid())return;
    if(parameter_dirty_){statusBar()->showMessage("Unsaved parameter edits: Apply or Revert before navigating.");return;}
    const int tab=item->data(0,Qt::UserRole).toInt(), row=item->data(0,Qt::UserRole+1).toInt();
    auto* list=tab==2?topic_list_:tab==3?parameter_list_:service_list_;
    list->setCurrentRow(row);tabs_->setCurrentIndex(tab);
}
void MainWindow::ShowTopic() {
    if(service_busy_)return;
    StopEcho();
    start_echo_->setEnabled(false);
    echo_table_->setRowCount(0);
    echo_status_->setText("Echo stopped");
    topic_identity_->clear();
    Clear(topic_fields_);
    const int row = topic_list_->currentRow();
    if (row < 0 || static_cast<std::size_t>(row) >= topics_.size()) return;
    const auto& t = topics_[row];
    const std::array<QString, 7> values{Text(t.name), Text(t.type_name), Number(t.payload_size) + " bytes",
        t.frequency_available?QString::number(t.frequency_hz, 'f', 1) + " Hz":QStringLiteral("N/A"),
        t.sequence_available?Number(t.sequence):QStringLiteral("N/A"), Names(t.publishers), Names(t.subscribers)};
    for (std::size_t i = 0; i < values.size(); ++i) topic_fields_[i]->setText(values[i]);
    topic_identity_->setText("Runtime PID " + QString::number(t.identity.runtime.pid) +
        " / start " + Number(t.identity.runtime.process_start_ticks) +
        " / endpoint " + Number(t.identity.registration_id) + " / type_id " + Number(t.type_id));
    if (!t.type_id) { echo_status_->setText("Type descriptor unavailable"); return; }
    if (t.role != "PUBLISHER" || t.publishers.empty()) {
        echo_status_->setText("Select a publisher endpoint to echo"); return;
    }
    if (echo_requires_refresh_) { echo_status_->setText("Refresh before starting Echo again."); return; }
    const int result = backend_->StartTopicEcho(t.identity);
    backend_->StopTopicEcho();
    if (result) { echo_status_->setText("Echo unavailable: " + ErrorText(result)); return; }
    start_echo_->setEnabled(true);
}
void MainWindow::StartEcho() {
    if(service_busy_)return;
    const int row = topic_list_->currentRow();
    if (!start_echo_->isEnabled() || row < 0 || static_cast<std::size_t>(row) >= topics_.size()) return;
    const auto identity = topics_[row].identity;
    const int result = backend_->StartTopicEcho(identity);
    if (result) {
        StopEcho();
        echo_requires_refresh_ = true;
        start_echo_->setEnabled(false);
        echo_status_->setText(result == -ESTALE
            ? QStringLiteral("Echo error: Topic was removed or recreated. Refresh and start Echo again.")
            : "Echo error: " + ErrorText(result));
        statusBar()->showMessage(echo_status_->text());
        return;
    }
    start_echo_->setEnabled(false);
    stop_echo_->setEnabled(true);
    echo_timer_->start();
    PollEcho();
}
void MainWindow::StopEcho() {
    echo_timer_->stop();
    backend_->StopTopicEcho();
    if (stop_echo_->isEnabled()) {
        start_echo_->setEnabled(true);
        echo_status_->setText("Echo stopped");
    }
    stop_echo_->setEnabled(false);
}
void MainWindow::PollEcho() {
    if(service_busy_)return;
    if (!echo_timer_->isActive()) return;
    DataSnapshot snapshot;
    const int result = backend_->ReadTopicEcho(snapshot);
    if (result == -EAGAIN) {
        echo_status_->setText("Echo waiting for a consistent sample — 100 ms display refresh");
        return;
    }
    if (result) {
        StopEcho();
        echo_requires_refresh_ = true;
        start_echo_->setEnabled(false);
        echo_status_->setText(result == -ESTALE
            ? QStringLiteral("Echo error: Topic was removed or recreated. Refresh and start Echo again.")
            : "Echo error: " + ErrorText(result));
        statusBar()->showMessage(echo_status_->text());
        return;
    }
    echo_table_->setRowCount(0);
    const auto append = [&](const QString& name, const FieldValue& field, const std::string& value) {
        const int row = echo_table_->rowCount();
        echo_table_->insertRow(row);
        echo_table_->setItem(row, 0, new QTableWidgetItem(name));
        echo_table_->setItem(row, 1, new QTableWidgetItem(Text(field.type_name)));
        echo_table_->setItem(row, 2, new QTableWidgetItem(Text(value)));
    };
    for (const auto& field : snapshot.fields) {
        if (field.array_values.empty()) append(Text(field.name), field, field.value);
        else for (std::size_t i = 0; i < field.array_values.size(); ++i)
            append(Text(field.name) + "[" + Number(i) + "]", field, field.array_values[i]);
    }
    topic_fields_[4]->setText(Number(snapshot.sequence));
    echo_status_->setText("Echo running — 100 ms display refresh / sequence " + Number(snapshot.sequence));
}
void MainWindow::ShowParameter() {
    if(service_busy_)return;
    const int row=parameter_list_->currentRow();
    if(parameter_dirty_){
        const QSignalBlocker blocker(parameter_list_);parameter_list_->setCurrentRow(parameter_row_);
        parameter_status_->setText("Unsaved edits: Apply or Revert before changing selection.");return;
    }
    parameter_row_=row;parameter_value_={};parameter_invalid_=true;
    parameter_rendering_=true;parameter_inputs_.clear();parameter_rows_.clear();parameter_table_->setRowCount(0);
    parameter_editor_->clear();parameter_editor_->setEnabled(false);parameter_editor_->show();parameter_rendering_=false;
    apply_button_->setEnabled(false);parameter_refresh_->setEnabled(false);parameter_revert_->setEnabled(false);
    Clear(parameter_fields_);parameter_metadata_->clear();parameter_status_->clear();
    if(row<0 || static_cast<std::size_t>(row)>=parameters_.size())return;
    const auto& p=parameters_[row];
    parameter_fields_[0]->setText(Text(p.name));parameter_fields_[1]->setText(Text(p.type_name));
    parameter_fields_[2]->setText(Text(p.value));parameter_fields_[3]->setText(p.writable?"Yes":"No (read-only)");
    parameter_metadata_->setText(Text(p.role)+" / PID "+QString::number(p.identity.runtime.pid)+
        " / start "+Number(p.identity.runtime.process_start_ticks)+" / endpoint "+Number(p.identity.registration_id)+
        " / type_id "+Number(p.type_id)+" / "+Number(p.payload_size)+" bytes / "+Text(p.diagnostic_type_name));
    if(!p.type_id){parameter_status_->setText("Type descriptor unavailable");return;}
    parameter_refresh_->setEnabled(true);RefreshParameterValue();
}
void MainWindow::RefreshParameterValue(){
    if(service_busy_)return;
    if(parameter_dirty_){parameter_status_->setText("Apply or Revert before Refresh Value.");return;}
    if(parameter_row_<0 || static_cast<std::size_t>(parameter_row_)>=parameters_.size())return;
    DataSnapshot value;const int r=backend_->GetParameter(parameters_[parameter_row_].identity,value);
    if(r){ParameterError("Get failed",r);return;}
    parameter_invalid_=false;RenderParameter(value);parameter_status_->setText("Current value loaded");
}
void MainWindow::RenderParameter(const DataSnapshot& value){
    parameter_rendering_=true;parameter_value_=value;parameter_dirty_=false;
    parameter_inputs_.clear();parameter_rows_.clear();parameter_table_->setRowCount(0);
    const bool single=value.fields.size()==1 && value.fields[0].array_values.empty();
    const bool writable=!parameter_invalid_ && parameters_[parameter_row_].writable;
    parameter_fields_[3]->setText(parameters_[parameter_row_].writable?(parameter_invalid_?"Unavailable until refresh":"Yes"):"No (read-only)");
    parameter_editor_->setVisible(single);parameter_editor_->setEnabled(single && writable);
    const auto add=[&](std::size_t field,int element,const std::string& text){
        const auto& f=value.fields[field];const int row=parameter_table_->rowCount();parameter_table_->insertRow(row);
        parameter_table_->setItem(row,0,new QTableWidgetItem(Text(f.name)+(element<0?QString():"["+QString::number(element)+"]")));
        parameter_table_->setItem(row,1,new QTableWidgetItem(Text(f.type_name)));
        parameter_table_->setItem(row,2,new QTableWidgetItem(Text(text)));
        auto* input=single?parameter_editor_:new QLineEdit;
        input->setAccessibleName(parameter_table_->item(row,0)->text());
        input->setText(Text(text));input->setEnabled(writable);
        if(single)parameter_table_->setItem(row,3,new QTableWidgetItem("Editor below"));
        else {parameter_table_->setCellWidget(row,3,input);connect(input,&QLineEdit::textChanged,this,&MainWindow::ParameterEdited);}
        parameter_inputs_.push_back(input);parameter_rows_.push_back({field,element});
    };
    FieldRows(value,add);
    parameter_fields_[2]->setText(single?Text(value.fields[0].value):QString::number(value.fields.size())+" fields (last Get)");
    apply_button_->setEnabled(false);parameter_revert_->setEnabled(false);parameter_refresh_->setEnabled(true);
    parameter_rendering_=false;
}
DataSnapshot MainWindow::EditedParameter() const{
    return EditedFields(parameter_value_,parameter_inputs_,parameter_rows_);
}
void MainWindow::ParameterEdited(){
    if(parameter_rendering_)return;
    parameter_dirty_=false;
    for(std::size_t row=0;row<parameter_inputs_.size();++row){auto [i,j]=parameter_rows_[row];const auto& f=parameter_value_.fields[i];
        if(parameter_inputs_[row]->text()!=Text(j<0?f.value:f.array_values[j]))parameter_dirty_=true;
    }
    apply_button_->setEnabled(parameter_dirty_ && !parameter_invalid_ && parameter_row_>=0 && parameters_[parameter_row_].writable);
    parameter_refresh_->setEnabled(!parameter_dirty_ && parameter_row_>=0 && parameters_[parameter_row_].type_id!=0);
    parameter_revert_->setEnabled(parameter_dirty_);
    if(parameter_invalid_)parameter_status_->setText("Value invalid/stale. Revert edits, Refresh, and reselect before applying.");
    else parameter_status_->setText(parameter_dirty_?"Edited — not applied":"Unchanged since last Get");
}
void MainWindow::RevertParameter(){
    if(parameter_row_<0)return;
    const auto value=parameter_value_;RenderParameter(value);
    parameter_status_->setText(parameter_invalid_?"Edits discarded. Refresh and reselect before applying.":"Edits discarded");
}
void MainWindow::ParameterError(const QString& action,int result){
    parameter_invalid_=true;apply_button_->setEnabled(false);
    parameter_fields_[3]->setText("Unavailable until refresh");
    const auto message=result==-ESTALE?QStringLiteral("Parameter is stale. Revert edits, Refresh, and reselect before applying."):
        action+": "+ErrorText(result)+" ("+QString::number(result)+"). Value not confirmed; refresh required.";
    parameter_status_->setText(message);statusBar()->showMessage(message);
}
void MainWindow::ApplyParameter() {
    if(service_busy_)return;
    if(!parameter_dirty_ || parameter_invalid_ || parameter_row_<0)return;
    const auto id=parameters_[parameter_row_].identity;
    const auto edited=EditedParameter();
    int result=backend_->ValidateParameter(id,edited);
    if(result){
        if(result==-EINVAL || result==-ERANGE || result==-EMSGSIZE){parameter_status_->setText("Invalid input: "+ErrorText(result));return;}
        ParameterError("Validation failed",result);return;
    }
    DataSnapshot current;result=backend_->GetParameter(id,current);
    if(result){ParameterError("Get before Apply failed",result);return;}
    if(current.type_id!=parameter_value_.type_id || current.type_name!=parameter_value_.type_name || current.fields.size()!=parameter_value_.fields.size()){
        ParameterError("Type changed",-EPROTOTYPE);return;
    }
    // Fresh whole value with only explicitly changed scalar/array elements overlaid.
    for(std::size_t row=0;row<parameter_rows_.size();++row){auto [i,j]=parameter_rows_[row];
        const auto& before=parameter_value_.fields[i];const auto& after=edited.fields[i];
        auto field=std::find_if(current.fields.begin(),current.fields.end(),[&](const auto& f){return f.name==before.name;});
        if(field==current.fields.end() || field->kind!=before.kind || field->type_name!=before.type_name || field->array_values.size()!=before.array_values.size()){
            ParameterError("Type changed",-EPROTOTYPE);return;
        }
        if(j<0){if(after.value!=before.value)field->value=after.value;}
        else if(after.array_values[j]!=before.array_values[j])field->array_values[j]=after.array_values[j];
    }
    result=backend_->SetParameter(id,current);
    if(result){ParameterError("Apply failed; edits not applied",result);return;}
    parameter_dirty_=false;
    DataSnapshot actual;result=backend_->GetParameter(id,actual);
    if(result){ParameterError("Set succeeded but readback failed",result);parameter_refresh_->setEnabled(true);return;}
    parameter_invalid_=false;RenderParameter(actual);
    parameter_status_->setText("Applied; current value re-read");statusBar()->showMessage("Updated "+Text(parameters_[parameter_row_].name));
}
void MainWindow::UpdateServices(){
    const bool had=service_row_>=0 && static_cast<std::size_t>(service_row_)<services_.size();
    const auto id=had?services_[service_row_].identity:EndpointIdentity{};
    services_=backend_->QueryServices();
    {const QSignalBlocker blocker(service_list_);service_list_->clear();int row=had?-1:(discovery_refreshed_ || services_.empty()?-1:0);
        for(std::size_t i=0;i<services_.size();++i){const auto& service=services_[i];
            service_list_->addItem(Text(service.name)+" [PID "+QString::number(service.identity.runtime.pid)+"]");
            if(had && service.identity==id)row=static_cast<int>(i);
        }service_list_->setCurrentRow(row);
    }ShowService();
}
void MainWindow::ShowService(){
    if(service_busy_){const QSignalBlocker blocker(service_list_);service_list_->setCurrentRow(service_row_);return;}
    service_row_=service_list_->currentRow();service_call_->setEnabled(false);service_template_={};
    service_inputs_.clear();service_rows_.clear();service_request_->setRowCount(0);service_response_->setRowCount(0);
    service_metadata_->clear();service_status_->clear();
    if(service_row_<0 || static_cast<std::size_t>(service_row_)>=services_.size())return;
    const auto& s=services_[service_row_];
    service_metadata_->setText(Text(s.name)+" / PID "+QString::number(s.identity.runtime.pid)+" / start "+Number(s.identity.runtime.process_start_ticks)+
        " / endpoint "+Number(s.identity.registration_id)+" / port "+Number(s.port)+" / service_id "+Number(s.service_id)+
        "\nRequest: "+Text(s.diagnostic_request_type_name)+" / type_id "+Number(s.request_type_id)+
        "\nResponse: "+Text(s.diagnostic_response_type_name)+" / type_id "+Number(s.response_type_id));
    if(!s.request_type_id || !s.response_type_id){service_status_->setText("Stable service type descriptor unavailable");return;}
    DataSnapshot request,response;int r=backend_->GetTypeTemplate(s.identity.runtime,s.request_type_id,request);
    if(!r)r=backend_->GetTypeTemplate(s.identity.runtime,s.response_type_id,response);
    if(r){service_status_->setText("Service unavailable: "+ErrorText(r));return;}
    service_template_=request;
    FieldRows(request,[&](std::size_t i,int j,const std::string& text){const auto& field=request.fields[i];
        const int row=service_request_->rowCount();service_request_->insertRow(row);
        const auto name=Text(field.name)+(j<0?QString():"["+QString::number(j)+"]");
        service_request_->setItem(row,0,new QTableWidgetItem(name));service_request_->setItem(row,1,new QTableWidgetItem(Text(field.type_name)));
        auto* input=new QLineEdit(Text(text));input->setAccessibleName(name);service_request_->setCellWidget(row,2,input);
        service_inputs_.push_back(input);service_rows_.push_back({i,j});
    });
    ResponseRows(service_response_,response,false);service_status_->setText("Ready — Call sends one request");service_call_->setEnabled(true);
}
void MainWindow::CallService(){
    if(service_busy_ || !service_call_->isEnabled() || service_row_<0)return;
    const auto id=services_[service_row_].identity;
    const auto request=EditedFields(service_template_,service_inputs_,service_rows_);
    service_response_->setRowCount(0);
    const int valid=backend_->ValidateService(id,request);
    if(valid){service_status_->setText("Request rejected: "+ErrorText(valid)+" ("+QString::number(valid)+")");return;}
    StopEcho(); // ToolBackend is serialized; no GUI operation races the worker.
    struct Result {int status{-EIO};DataSnapshot response;};auto result=std::make_shared<Result>();
    service_thread_=QThread::create([this,id,request,result]{
        try {result->status=backend_->CallService(id,request,result->response,200,2);}
        catch(const std::bad_alloc&){result->status=-ENOMEM;}catch(...){result->status=-EIO;}
    });
    service_thread_->setParent(this);
    connect(service_thread_,&QThread::finished,this,[this,result]{
        service_thread_->wait();service_thread_->deleteLater();service_thread_=nullptr;
        service_busy_=false;tabs_->setEnabled(true);service_call_->setEnabled(true);
        if(!result->status){ResponseRows(service_response_,result->response,true);service_status_->setText("Call succeeded");}
        else {service_response_->setRowCount(0);
            service_status_->setText(result->status==-ETIMEDOUT?
                QStringLiteral("Call timed out: no response received. Server callback may have executed. No automatic retry."):
                "Call failed: "+ErrorText(result->status)+" ("+QString::number(result->status)+")");
        }statusBar()->showMessage(service_status_->text());
    },Qt::QueuedConnection);
    service_busy_=true;tabs_->setEnabled(false);service_call_->setEnabled(false);
    service_status_->setText("Call in progress");statusBar()->showMessage("Service call in progress — awaiting response");
    service_thread_->start();
}

}
