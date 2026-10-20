#pragma once
#include "kcf_tool/backend/tool_backend.hpp"
#include <QWidget>
#include <memory>
class QTabWidget;
namespace kcf_tool {
class GraphWidget;
class RemoteWorkspace final:public QWidget {
public:
    RemoteWorkspace(ToolBackend&,QTabWidget&,GraphWidget&,QWidget* parent=nullptr);
    ~RemoteWorkspace() override;
    bool BeginClose();
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
