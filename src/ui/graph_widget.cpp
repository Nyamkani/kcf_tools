#include "kcf_tool/ui/graph_widget.hpp"
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPainterPath>
#include <QAction>
#include <QFontMetrics>
#include <map>
#include <algorithm>

namespace kcf_tool {
class GraphView final : public QGraphicsView {
public:
    using QGraphicsView::QGraphicsView;
    void Zoom(qreal factor) {
        const auto current=transform().m11();
        const auto next=std::clamp(current*factor,qreal(0.15),qreal(4.0));
        if(current>0)scale(next/current,next/current);
    }
protected:
    void wheelEvent(QWheelEvent* event) override {
        Zoom(event->angleDelta().y()>0?1.15:1/1.15);event->accept();
    }
    void keyPressEvent(QKeyEvent* event) override {
        if(event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter) {
            const auto items=scene()->selectedItems();
            if(!items.empty())activate(items.front());
            event->accept();return;
        }
        QGraphicsView::keyPressEvent(event);
    }
public:
    std::function<void(QGraphicsItem*)> activate;
};
namespace {
class NodeItem final : public QGraphicsRectItem {
public:
    NodeItem(const QRectF& rect,std::function<void()> action):QGraphicsRectItem(rect),action_(std::move(action)) {
        setFlag(ItemIsSelectable);setCursor(Qt::PointingHandCursor);
    }
protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override {event->accept();action_();}
private:
    std::function<void()> action_;
};
QString Text(const std::string& value){return QString::fromStdString(value);}
}
GraphWidget::GraphWidget(QWidget* parent):QWidget(parent) {
    setObjectName("graphTab");auto* layout=new QVBoxLayout(this);auto* controls=new QHBoxLayout;
    const auto button=[&](const char* label,const char* name,auto action){auto* b=new QPushButton(label);b->setObjectName(name);
        controls->addWidget(b);connect(b,&QPushButton::clicked,this,action);};
    button("Refresh Graph","refreshGraphButton",[this]{if(refresh)refresh();});
    const auto toggle=[&](const char* label,const char* name,bool checked){auto* t=new QCheckBox(label);t->setObjectName(name);
        t->setChecked(checked);controls->addWidget(t);connect(t,&QCheckBox::toggled,this,[this]{Draw();});return t;};
    topics_toggle_=toggle("Topics","graphTopics",true);
    parameters_toggle_=toggle("Parameters","graphParameters",false);
    services_toggle_=toggle("Services","graphServices",false);
    actions_toggle_=toggle("Actions","graphActions",false);
    local_toggle_=toggle("Local","graphLocal",true);
    remote_toggle_=toggle("Remote","graphRemote",true);
    // Filters use a second row so controls remain usable on laptop screens.
    layout->addLayout(controls);controls=new QHBoxLayout;
    controls->addWidget(new QLabel("Host"));host_filter_=new QComboBox;host_filter_->setObjectName("graphHostFilter");
    host_filter_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);host_filter_->setMinimumContentsLength(18);host_filter_->setMaximumWidth(300);
    host_filter_->addItem("All",QString());controls->addWidget(host_filter_);
    connect(host_filter_,&QComboBox::currentIndexChanged,this,[this]{Draw();});
    controls->addWidget(new QLabel("Application"));filter_=new QComboBox;filter_->setObjectName("graphApplicationFilter");
    filter_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);filter_->setMinimumContentsLength(18);filter_->setMaximumWidth(350);
    filter_->addItem("All",QString());controls->addWidget(filter_);
    connect(filter_,&QComboBox::currentIndexChanged,this,[this]{Draw();});
    controls->addStretch();
    button("−","graphZoomOut",[this]{view_->Zoom(1/1.2);});
    button("+","graphZoomIn",[this]{view_->Zoom(1.2);});
    button("Fit","graphFit",[this]{view_->fitInView(scene_->sceneRect(),Qt::KeepAspectRatio);});
    layout->addLayout(controls);
    layout->addWidget(new QLabel("Double-click / Enter: open Element or Topic • Wheel: zoom • Drag background: pan"));
    scene_=new QGraphicsScene(this);scene_->setObjectName("graphScene");
    view_=new GraphView(scene_,this);view_->setObjectName("graphView");
    view_->setRenderHint(QPainter::Antialiasing);view_->setDragMode(QGraphicsView::ScrollHandDrag);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);layout->addWidget(view_);
    view_->activate=[this](QGraphicsItem* item){const auto id=item->data(0).toString();
        for(const auto& node:model_.nodes)if(Text(node.id)==id){Activate(node);return;}};
    detail_=new QPlainTextEdit;detail_->setReadOnly(true);detail_->setObjectName("graphRemoteDetail");
    detail_->setMaximumHeight(180);detail_->hide();layout->addWidget(detail_);
    connect(scene_,&QGraphicsScene::selectionChanged,this,[this]{
        detail_->clear();detail_->hide();
        const auto items=scene_->selectedItems();if(items.empty())return;
        const auto id=items.front()->data(0).toString();
        for(const auto& node:model_.nodes)if(Text(node.id)==id && !node.host.empty()){
            detail_->setPlainText(Text(node.detail));detail_->show();return;
        }
    });
    status_=new QLabel;status_->setObjectName("graphStatus");layout->addWidget(status_);
}
void GraphWidget::SetSnapshot(const std::vector<ApplicationInfo>& applications,const std::vector<ElementInfo>& elements,
                              const std::vector<TopicInfo>& topics,const std::vector<ParameterInfo>& parameters,
                              const std::vector<ServiceInfo>& services,const NetworkSnapshot& network) {
    applications_=applications;elements_=elements;topics_=topics;parameters_=parameters;services_=services;network_=network;
    detail_->clear();detail_->hide();
    const auto all=AddRemoteGraph(BuildGraph(applications_,elements_,topics_,parameters_,services_),network_);
    {const QSignalBlocker blocker(host_filter_);const auto previous=host_filter_->currentData();
        host_filter_->clear();host_filter_->addItem("All",QString());host_filter_->addItem("Local", "local");
        for(const auto& host:network_.hosts)host_filter_->addItem(Text(host.label+" ["+host.id+"]"),Text(host.id));
        const int index=host_filter_->findData(previous);host_filter_->setCurrentIndex(index<0?0:index);}
    {const QSignalBlocker blocker(filter_);const auto previous=filter_->currentData();filter_->clear();filter_->addItem("All",QString());
        for(const auto& group:all.groups)filter_->addItem(Text(group.label),Text(group.id));
        const int index=filter_->findData(previous);filter_->setCurrentIndex(index<0?0:index);}
    Draw();
}
void GraphWidget::Draw() {
    const GraphOptions options{topics_toggle_->isChecked(),parameters_toggle_->isChecked(),services_toggle_->isChecked(),
        filter_->currentData().toString().toStdString(),actions_toggle_->isChecked(),local_toggle_->isChecked(),remote_toggle_->isChecked(),
        host_filter_->currentData().toString().toStdString()};
    model_=AddRemoteGraph(BuildGraph(applications_,elements_,topics_,parameters_,services_,options),network_,options);
    detail_->clear();detail_->hide();
    scene_->clear();std::map<std::string,QRectF> boxes;
    qreal y=40;
    const auto node_item=[&](const GraphNode& node,qreal x,qreal top) {
        const bool element=node.kind==GraphKind::Element;const qreal width=node.kind==GraphKind::Host?830:element?270:240,height=element?58:38;
        auto* item=new NodeItem({0,0,width,height},[this,node]{Activate(node);});
        if(node.host.empty() && !element && node.kind!=GraphKind::Topic)item->setCursor(Qt::ArrowCursor);
        item->setPos(x,top);item->setData(0,Text(node.id));item->setData(1,static_cast<int>(node.kind));
        item->setBrush(element?QColor("#e3edf8"):node.kind==GraphKind::Topic?QColor("#dff3e8"):
            node.kind==GraphKind::Parameter?QColor("#fff0d1"):QColor("#eee3f6"));
        item->setPen(QPen(QColor("#526477"),1,node.lost?Qt::DashLine:Qt::SolidLine));scene_->addItem(item);
        auto* label=new QGraphicsSimpleTextItem(item);label->setAcceptedMouseButtons(Qt::NoButton);
        label->setText(QFontMetrics(label->font()).elidedText(Text(node.label),Qt::ElideMiddle,int(width-20)));label->setPos(10,8);
        QString tooltip=Text(node.label);
        if(element && node.host.empty()){auto* identity=new QGraphicsSimpleTextItem(item);identity->setAcceptedMouseButtons(Qt::NoButton);
            identity->setText(QString("PID %1 / start %2").arg(node.runtime.pid).arg(node.runtime.process_start_ticks));identity->setPos(10,31);}
        else for(const auto& endpoint:node.endpoints)tooltip+=QString("\n%1 PID %2 / start %3 / registration %4")
            .arg(Text(endpoint.role)).arg(endpoint.identity.runtime.pid).arg(endpoint.identity.runtime.process_start_ticks).arg(endpoint.identity.registration_id);
        item->setToolTip(node.host.empty()?tooltip:QString("<pre>%1</pre>").arg(Text(node.detail).toHtmlEscaped()));boxes[node.id]={x,top,width,height};
    };
    for(const auto& group:model_.groups) {
        if(!group.host.empty())continue;
        if(!filter_->currentData().toString().isEmpty() && Text(group.id)!=filter_->currentData().toString())continue;
        const auto start=y;auto* title=scene_->addSimpleText(Text(group.label));title->setPos(10,y);y+=30;
        for(const auto& node:model_.nodes)if(node.kind==GraphKind::Element && node.group==group.id){node_item(node,20,y);y+=82;}
        if(y==start+30)y+=30;
        auto* frame=scene_->addRect(0,start-8,310,y-start+4,QPen(QColor("#aab8c5")),QBrush(QColor("#f6f8fa")));frame->setZValue(-2);y+=24;
    }
    qreal endpoint_y=40;GraphKind previous=GraphKind::Element;
    for(const auto& node:model_.nodes)if(node.host.empty() && node.kind!=GraphKind::Element){
        if(node.kind!=previous){auto* title=scene_->addSimpleText(node.kind==GraphKind::Topic?"Topics":node.kind==GraphKind::Parameter?"Parameters":"Service servers");
            title->setPos(600,endpoint_y);endpoint_y+=30;previous=node.kind;}
        node_item(node,600,endpoint_y);endpoint_y+=66;
    }
    // Remote sections are nested Host -> Application -> Runtime / Endpoint.
    // Host identity (not a label/IP) scopes every registration.
    y=std::max(y,endpoint_y)+30;
    for(const auto& host:model_.nodes)if(host.kind==GraphKind::Host){
        const auto host_start=y;node_item(host,0,y);y+=60;
        for(const auto& group:model_.groups)if(group.host==host.host){
            if(!options.group.empty() && options.group!=group.id)continue;
            const auto start=y;auto* title=scene_->addSimpleText(Text(group.label));title->setPos(20,y);y+=30;
            auto right=y;
            for(const auto& node:model_.nodes)if(node.group==group.id){
                if(node.kind==GraphKind::Element){node_item(node,30,y);y+=82;}
                else{node_item(node,600,right);right+=66;}
            }
            y=std::max(y,right);
            auto* frame=scene_->addRect(10,start-5,850,y-start+8,QPen(QColor("#aab8c5")));frame->setZValue(-2);y+=30;
        }
        auto* frame=scene_->addRect(-10,host_start-10,890,y-host_start+10,QPen(QColor("#617386"),2,host.lost?Qt::DashLine:Qt::SolidLine));
        frame->setZValue(-3);y+=40;
    }
    for(const auto& edge:model_.edges) {
        const auto from=boxes.at(edge.from),to=boxes.at(edge.to);const bool forward=from.left()<to.left();
        // Separate outgoing and incoming ports to keep bidirectional relations visible.
        const QPointF a(forward?from.right():from.left(),from.center().y()+(forward?-7:7));
        const QPointF b(forward?to.left():to.right(),to.center().y()+(forward?-7:7));
        const qreal direction=forward?1:-1;
        QPainterPath path(a);path.cubicTo(a+QPointF(110*direction,0),b-QPointF(110*direction,0),b);
        const bool ownership=edge.role.find("metadata ownership")!=std::string::npos;
        auto* line=scene_->addPath(path,QPen(QColor("#617386"),1.6,ownership?Qt::DashLine:Qt::SolidLine));line->setZValue(-1);
        line->setToolTip(Text(edge.role)+QString(" / registration %1").arg(edge.endpoint.registration_id));
        QPolygonF arrow;arrow<<b<<b+QPointF(-9*direction,-4)<<b+QPointF(-9*direction,4);
        auto* head=scene_->addPolygon(arrow,QPen(Qt::NoPen),QBrush(QColor("#617386")));head->setZValue(-1);
    }
    scene_->setSceneRect(scene_->itemsBoundingRect().adjusted(-24,-24,24,24));
    status_->setText(QString("%1 nodes • %2 metadata relations • ").arg(model_.nodes.size()).arg(model_.edges.size())+Text(network_.status));
    status_->setToolTip(Text(network_.route_status));
}
void GraphWidget::Activate(const GraphNode& node) {
    if(!node.host.empty()){detail_->setPlainText(Text(node.detail));detail_->show();return;}
    if(node.kind==GraphKind::Element){if(open_element)open_element(node.runtime);return;}
    if(node.kind!=GraphKind::Topic || !open_topic)return;
    if(node.endpoints.size()==1){open_topic(node.endpoints.front().identity);return;}
    // Non-modal menu: Refresh can delete scene items without leaving references
    // into an obsolete snapshot on the stack. MainWindow rechecks the identity.
    auto* menu=new QMenu(this);menu->setAttribute(Qt::WA_DeleteOnClose);
    for(const auto& endpoint:node.endpoints){auto* action=menu->addAction(QString("%1 — PID %2 / start %3 / endpoint %4")
        .arg(Text(endpoint.role)).arg(endpoint.identity.runtime.pid).arg(endpoint.identity.runtime.process_start_ticks).arg(endpoint.identity.registration_id));
        connect(action,&QAction::triggered,this,[this,id=endpoint.identity]{if(open_topic)open_topic(id);});}
    menu->popup(QCursor::pos());
}
}
