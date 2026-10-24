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
    parser.addOption({"no-network", "Disable read-only Network discovery"});
    parser.addOption({"network-interface", "Discovery IPv4 interface", "address", "0.0.0.0"});
    parser.addOption({"network-port", "Discovery UDP port", "port", "37651"});
    parser.addOption({"network-host-id", "Persistent Tool Host ID (32 hex digits); must match Gateway allowlists", "id"});
    parser.addOption({"gateway-socket", "Read-only host-local Gateway socket", "path", "/run/kcf/gateway.sock"});
    parser.process(app);
    const auto selected = parser.value("backend");
    if (selected != "mock" && selected != "kcf") parser.showHelp(1);
    std::unique_ptr<kcf_tool::ToolBackend> backend;
    if (selected == "kcf") {
        auto real=std::make_unique<kcf_tool::KcfBackend>();
        real->ConfigureLocalGateway(parser.value("gateway-socket").toStdString(),true);
        if(!parser.isSet("no-network")) {
            bool ok=false;const auto port=parser.value("network-port").toUInt(&ok);
            if(!ok || !port || port>65535)parser.showHelp(1);
            real->StartNetwork({parser.value("network-interface").toStdString(),static_cast<std::uint16_t>(port),parser.value("network-host-id").toStdString()});
        }
        backend=std::move(real);
    }
    else backend = std::make_unique<kcf_tool::MockBackend>();
    kcf_tool::MainWindow window(std::move(backend), selected == "kcf" ? "KCF" : "Mock");
    window.resize(1100, 650);
    window.show();
    return app.exec();
}
