#include "kcf_tool/backend/mock_backend.hpp"
#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf_tool/ui/main_window.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <memory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("KCF Tool");
    parser.addHelpOption();
    parser.addOption({"backend", "Backend: mock or kcf", "name", "mock"});
    parser.process(app);
    const auto selected = parser.value("backend");
    if (selected != "mock" && selected != "kcf") parser.showHelp(1);
    std::unique_ptr<kcf_tool::ToolBackend> backend;
    if (selected == "kcf") backend = std::make_unique<kcf_tool::KcfBackend>();
    else backend = std::make_unique<kcf_tool::MockBackend>();
    kcf_tool::MainWindow window(std::move(backend), selected == "kcf" ? "KCF" : "Mock");
    window.resize(1100, 650);
    window.show();
    return app.exec();
}
