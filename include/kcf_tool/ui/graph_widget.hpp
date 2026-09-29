#pragma once
#include "kcf_tool/ui/graph_model.hpp"
#include <QWidget>
#include <functional>
class QCheckBox;
class QComboBox;
class QGraphicsScene;
class QLabel;
class QPlainTextEdit;
namespace kcf_tool {
class GraphView;
class GraphWidget final : public QWidget {
public:
    explicit GraphWidget(QWidget* parent=nullptr);
    void SetSnapshot(const std::vector<ApplicationInfo>&,const std::vector<ElementInfo>&,
                     const std::vector<TopicInfo>&,const std::vector<ParameterInfo>&,
                     const std::vector<ServiceInfo>&, const NetworkSnapshot& = {});
    const GraphModel& Model() const { return model_; }
    std::function<void()> refresh;
    std::function<void(RuntimeIdentity)> open_element;
    std::function<void(EndpointIdentity)> open_topic;
private:
    void Draw();
    void Activate(const GraphNode&);
    std::vector<ApplicationInfo> applications_;
    std::vector<ElementInfo> elements_;
    std::vector<TopicInfo> topics_;
    std::vector<ParameterInfo> parameters_;
    std::vector<ServiceInfo> services_;
    NetworkSnapshot network_;
    GraphModel model_;
    QCheckBox *actions_toggle_, *local_toggle_, *remote_toggle_;
    QComboBox* host_filter_;
    QPlainTextEdit* detail_;
    QCheckBox *topics_toggle_, *parameters_toggle_, *services_toggle_;
    QComboBox* filter_;
    GraphView* view_;
    QGraphicsScene* scene_;
    QLabel* status_;
};
}
