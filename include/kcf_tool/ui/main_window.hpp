#pragma once
#include "kcf_tool/backend/tool_backend.hpp"
#include <QMainWindow>
#include <array>
#include <memory>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QListWidget;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTimer;
class QCloseEvent;
class QThread;
class QTabWidget;

namespace kcf_tool {
class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(std::unique_ptr<ToolBackend> backend,
                        const QString& backend_name = QStringLiteral("Backend"),
                        QWidget* parent = nullptr);
    ~MainWindow() override;
private:
    void closeEvent(QCloseEvent*) override;
    void StartEcho();
    void StopEcho();
    void PollEcho();
    void Refresh();
    void ShowElement();
    void UpdateExplorer();
    void OpenElementEndpoint(QTreeWidgetItem*);
    void ShowTopic();
    void ShowParameter();
    void UpdateServices();
    void ShowService();
    void CallService();
    void ApplyParameter();
    void RefreshParameterValue();
    void RenderParameter(const DataSnapshot&);
    void ParameterEdited();
    void RevertParameter();
    void ParameterError(const QString&, int);
    DataSnapshot EditedParameter() const;

    std::unique_ptr<ToolBackend> backend_;
    QTabWidget* tabs_;
    std::vector<ServiceInfo> services_;
    QListWidget* service_list_;
    QLabel* service_metadata_;
    QLabel* service_status_;
    QPushButton* service_call_;
    QTableWidget* service_request_;
    QTableWidget* service_response_;
    DataSnapshot service_template_;
    std::vector<QLineEdit*> service_inputs_;
    std::vector<std::pair<std::size_t,int>> service_rows_;
    int service_row_{-1};
    bool service_busy_{false};
    QThread* service_thread_{nullptr};
    std::vector<ApplicationInfo> applications_;
    std::vector<ElementInfo> elements_;
    std::vector<TopicInfo> topics_;
    std::vector<ParameterInfo> parameters_;
    QLabel* application_status_;
    QLabel* connection_status_;
    std::array<QLabel*, 7> application_fields_{};
    std::array<QLabel*, 7> element_fields_{};
    std::array<QLabel*, 7> topic_fields_{};
    std::array<QLabel*, 4> parameter_fields_{};
    QTreeWidget* application_tree_;
    QTreeWidget* element_endpoints_;
    QLabel* element_identity_;
    QListWidget* element_list_;
    QListWidget* topic_list_;
    QListWidget* parameter_list_;
    QLineEdit* parameter_editor_;
    QPushButton* apply_button_;
    QPushButton* parameter_refresh_;
    QPushButton* parameter_revert_;
    QLabel* parameter_metadata_;
    QLabel* parameter_status_;
    QTableWidget* parameter_table_;
    std::vector<QLineEdit*> parameter_inputs_;
    std::vector<std::pair<std::size_t,int>> parameter_rows_;
    DataSnapshot parameter_value_;
    int parameter_row_{-1};
    bool parameter_dirty_{false}, parameter_invalid_{true}, parameter_rendering_{false};
    QPushButton* start_echo_;
    QPushButton* stop_echo_;
    QLabel* echo_status_;
    QLabel* topic_identity_;
    QTableWidget* echo_table_;
    QTimer* echo_timer_;
    bool echo_requires_refresh_{false};
    bool discovery_refreshed_{false};

};
}
