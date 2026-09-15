#include "kcf_tool/backend/mock_backend.hpp"
#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf_tool/ui/main_window.hpp"
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QTableWidget>
#include <QElapsedTimer>
#include <QThread>
#include <cerrno>
#include <cstdlib>
#include <iostream>

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    auto backend = std::make_unique<kcf_tool::MockBackend>();
    auto* mock = backend.get();
    Check(mock->GetApplicationInfo().name == "mecanum", "application data");
    Check(mock->SetParameter("missing", "value") == -ENOENT, "missing parameter");
    Check(mock->SetParameter("/mecanum/motor/serial_port", "changed") == -EPERM, "read-only backend");
    Check(mock->GetParameters()[2].value == "/dev/ttyUSB1", "read-only unchanged");
    kcf_tool::MainWindow window(std::move(backend), "Mock");
    window.show();
    QTimer::singleShot(0, &window, [&] {
        Check(window.findChild<QLabel*>("backendStatus")->text() == "Backend: Mock", "mock backend label");
        Check(window.findChild<QLabel*>("connectionStatus")->text() == "Connection: Connected", "mock connection label");
        auto* tabs = window.findChild<QTabWidget*>();
        Check(window.isVisible() && tabs->count() == 6, "window and six tabs");
        const QStringList titles{"Applications", "Elements", "Topics", "Parameters", "Launch", "Services"};
        for (int i = 0; i < titles.size(); ++i) {
            tabs->setCurrentIndex(i);
            Check(tabs->tabText(i) == titles[i], "tab title");
        }
        auto* elements = window.findChild<QListWidget*>("elementsList");
        auto* topics = window.findChild<QListWidget*>("topicsList");
        auto* parameters = window.findChild<QListWidget*>("parametersList");
        Check(elements->count() == 3 && topics->count() == 3 && parameters->count() == 3, "mock lists");
        auto hasText = [&](int tab, const QString& text) {
            for (auto* label : tabs->widget(tab)->findChildren<QLabel*>())
                if (label->text() == text) return true;
            return false;
        };
        Check(hasText(0, "mecanum") && hasText(0, "RUNNING"), "application details");
        elements->setCurrentRow(1);
        Check(hasText(1, "kcf_mecanum_imu") && hasText(1, "12002"), "element selection details");
        topics->setCurrentRow(1);
        Check(hasText(2, "mecanum::OdometryData") && hasText(2, "localization"), "topic selection details");
        parameters->setCurrentRow(1);
        auto* editor = window.findChild<QLineEdit*>("parameterEditor");
        QPushButton* apply = nullptr;
        for (auto* button : tabs->widget(3)->findChildren<QPushButton*>())
            if (button->text() == "Apply") apply = button;
        Check(apply && !apply->isEnabled() && editor->isEnabled(), "writable editor");
        editor->setText("4294967296");apply->click();
        Check(mock->GetParameters()[1].value=="15", "invalid uint32 not written");
        parameters->setCurrentRow(0);
        Check(parameters->currentRow()==1, "dirty selection blocked");
        window.findChild<QPushButton*>("refreshButton")->click();
        Check(editor->text()=="4294967296", "dirty refresh preserves edit");
        window.findChild<QPushButton*>("revertParameterButton")->click();
        Check(editor->text()=="15" && !apply->isEnabled(), "revert restores baseline");
        editor->setText("25");
        apply->click();
        Check(mock->GetParameters()[1].value == "25" && hasText(3, "25"), "apply updates backend and detail");
        parameters->setCurrentRow(2);
        Check(!editor->isEnabled() && !apply->isEnabled(), "read-only controls disabled");
        Check(window.findChild<QLabel*>("parameterStatus")->text()=="Type descriptor unavailable", "undefined parameter disabled");
        const auto heartbeat = mock->GetElements()[1].heartbeat;
        window.findChild<QPushButton*>("refreshButton")->click();
        Check(mock->GetElements()[1].heartbeat == heartbeat + 1, "refresh heartbeat");
        Check(elements->currentRow() == 1 && topics->currentRow() == 1 && parameters->currentRow() == 2,
              "refresh preserves selections");
        Check(window.findChild<QLabel*>("elementHeartbeat")->text().toULongLong() == heartbeat + 1,
              "refresh updates selected detail");
        for (auto* button : tabs->widget(4)->findChildren<QPushButton*>())
            Check(!button->isEnabled(), "launch disabled");
        auto* start = window.findChild<QPushButton*>("startEchoButton");
        auto* stop = window.findChild<QPushButton*>("stopEchoButton");
        auto* timer = window.findChild<QTimer*>("echoTimer");
        auto* fields = window.findChild<QTableWidget*>("echoFields");
        Check(start->isEnabled() && !stop->isEnabled(), "echo ready");
        auto wait = [&] {
            QElapsedTimer elapsed; elapsed.start();
            while(elapsed.elapsed() < 250) { app.processEvents(); QThread::msleep(5); }
        };
        start->click();
        Check(timer->isActive() && timer->interval() == 100, "100 ms timer");
        Check(fields->rowCount() == 6 && fields->item(1,2)->text() == "2.25", "mock scalar");
        Check(fields->item(5,0)->text() == "values[3]" && fields->item(5,2)->text() == "8", "mock array");
        auto seq = fields->item(0,2)->text().toULongLong(); wait();
        Check(fields->item(0,2)->text().toULongLong() > seq, "poll updates");
        stop->click(); seq = mock->GetTopics()[1].sequence; wait();
        Check(!timer->isActive() && mock->GetTopics()[1].sequence == seq, "stop prevents reads");
        start->click(); topics->setCurrentRow(0);
        Check(!timer->isActive() && fields->rowCount() == 0, "switch clears old target");
        start->click();
        Check(fields->item(1,2)->text() == "1.25", "new topic data");
        window.findChild<QPushButton*>("refreshButton")->click();
        Check(!timer->isActive() && fields->rowCount() == 0, "refresh stops echo");
        Check(window.statusBar()->currentMessage() == "Last refresh: OK", "refresh status");
        kcf_tool::MainWindow disconnected(std::make_unique<kcf_tool::KcfBackend>(), "KCF");
        disconnected.show();
        Check(disconnected.findChild<QLabel*>("backendStatus")->text() == "Backend: KCF", "KCF backend label");
        Check(disconnected.findChild<QLabel*>("connectionStatus")->text() == "Connection: Disconnected", "KCF connection label");
        Check(disconnected.findChild<QTabWidget*>()->count() == 6, "KCF retains tabs");
        for (auto* list : disconnected.findChildren<QListWidget*>())
            Check(list->count() == 0, "KCF GUI has no fake data");
        Check(!disconnected.findChild<QLineEdit*>("parameterEditor")->isEnabled(), "KCF editor disabled");
        for (auto* button : disconnected.findChildren<QPushButton*>())
            if (button->text() == "Apply") Check(!button->isEnabled(), "KCF apply disabled");
        disconnected.findChild<QPushButton*>("refreshButton")->click();
        Check(disconnected.statusBar()->currentMessage() == "Last refresh: Not supported", "KCF unsupported status");
        auto* service_list=window.findChild<QListWidget*>("servicesList");
        auto* request=window.findChild<QTableWidget*>("serviceRequest");
        auto* response=window.findChild<QTableWidget*>("serviceResponse");
        auto* call=window.findChild<QPushButton*>("callServiceButton");
        Check(service_list->count()==2 && request->rowCount()==2,"mock service discovery/form");
        qobject_cast<QLineEdit*>(request->cellWidget(0,2))->setText("2");
        qobject_cast<QLineEdit*>(request->cellWidget(1,2))->setText("3");
        call->click();Check(!call->isEnabled(),"call disabled in flight");
        QElapsedTimer service_wait;service_wait.start();
        while(!call->isEnabled()){Check(service_wait.elapsed()<5000,"mock async completion");app.processEvents();QThread::msleep(5);}
        Check(response->rowCount()==1 && response->item(0,2)->text()=="5","mock service result");
        service_list->setCurrentRow(1);
        Check(response->item(0,2)->text().isEmpty() && qobject_cast<QLineEdit*>(request->cellWidget(0,2))->text()=="0","service switch clears values");
        disconnected.close();
        start->click();
        Check(timer->isActive(), "echo before close");
        window.close();
        Check(!timer->isActive(), "close stops echo");
        app.quit();
    });
    const int result = app.exec();
    Check(result == 0, "clean event-loop exit");
    std::cout << "PASS: mock backend and widget interactions\n";
    return result;
}
