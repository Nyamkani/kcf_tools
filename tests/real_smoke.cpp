#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf_tool/ui/main_window.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <QStatusBar>
#include <csignal>
#include <cassert>

int main(int argc,char** argv) {
    QApplication app(argc,argv); assert(argc==2);
    const QString prefix = "/kt4_qt_"+QString::number(QCoreApplication::applicationPid());
    QProcess fixture; fixture.start(argv[1],{"--element",prefix}); assert(fixture.waitForStarted());
    kcf_tool::MainWindow window(std::make_unique<kcf_tool::KcfBackend>(),"KCF"); window.show();
    auto* refresh=window.findChild<QPushButton*>("refreshButton");
    auto* connection=window.findChild<QLabel*>("connectionStatus");
    auto* topics=window.findChild<QListWidget*>("topicsList");
    auto* start=window.findChild<QPushButton*>("startEchoButton");
    auto* stop=window.findChild<QPushButton*>("stopEchoButton");
    auto* timer=window.findChild<QTimer*>("echoTimer");
    auto* fields=window.findChild<QTableWidget*>("echoFields");
    auto* status=window.findChild<QLabel*>("echoStatus");
    const auto wait = [&](auto condition) {
        QElapsedTimer elapsed; elapsed.start();
        while(!condition()) { assert(elapsed.elapsed()<10000); app.processEvents(); QThread::msleep(5); }
    };
    const auto select = [&](const QString& suffix, const QString& role) {
        for(int i=0;i<topics->count();++i) {
            const auto text=topics->item(i)->text();
            if(text.startsWith(prefix+suffix+" ["+role)) { topics->setCurrentRow(i); return true; }
        }
        return false;
    };
    wait([&]{refresh->click(); return select("/topic","PUBLISHER") && start->isEnabled() && topics->count()==3 && window.findChild<QListWidget*>("parametersList")->count()==2;});
    assert(connection->text()=="Connection: Connected");
    assert(window.findChild<QTabWidget*>()->count()==6);
    window.findChild<QListWidget*>("elementsList")->setCurrentRow(0);
    assert(window.findChild<QLabel*>("elementHeartbeat")->text()=="N/A");
    assert(window.findChild<QLabel*>("topicFrequency")->text()=="N/A");
    assert(window.findChild<QLabel*>("topicSequence")->text()=="N/A");
    auto* parameters=window.findChild<QListWidget*>("parametersList");
    assert(parameters->count()==2); parameters->setCurrentRow(0);
    assert(!window.findChild<QLineEdit*>("parameterEditor")->isEnabled());
    start->click(); assert(timer->isActive());
    wait([&]{return fields->rowCount()==7;});
    assert(window.findChild<QLabel*>("topicSequence")->text()!="N/A");
    assert(window.findChild<QLabel*>("topicFrequency")->text()=="N/A");
    assert(fields->item(0,2)->text()=="7" && fields->item(1,2)->text()=="1.25");
    assert(fields->item(6,0)->text()=="values[3]" && fields->item(6,2)->text()=="8");
    assert(::kill(static_cast<pid_t>(fixture.processId()),SIGUSR1)==0);
    wait([&]{return fields->item(1,2)->text()=="9.5";});
    assert(fields->item(0,2)->text()=="8" && fields->item(6,2)->text()=="40");
    // Publish A again by restarting this fixture later; current x is 9.5.
    // Same-process recreation must stop this session even when B has equal bytes.
    const auto before_recreate=window.findChild<QLabel*>("topicIdentity")->text();
    assert(::kill(static_cast<pid_t>(fixture.processId()),SIGWINCH)==0);
    wait([&]{return !timer->isActive();});
    assert(status->text().contains("Refresh and start Echo again"));
    assert(!start->isEnabled());
    assert(fields->item(1,2)->text()=="9.5");
    wait([&]{refresh->click(); return select("/topic","PUBLISHER") && start->isEnabled() && topics->count()==3 && window.findChild<QListWidget*>("parametersList")->count()==2;});
    assert(window.findChild<QLabel*>("topicIdentity")->text()!=before_recreate);
    start->click(); assert(timer->isActive());
    wait([&]{return fields->rowCount()==7;});
    assert(fields->item(1,2)->text()=="9.5");
    stop->click(); assert(!timer->isActive());
    // Duplicate name, different endpoint: selecting subscriber must not silently
    // resolve by name to the publisher. Refresh must preserve that exact choice.
    assert(select("/topic","SUBSCRIBER")); assert(!start->isEnabled());
    const auto selected=window.findChild<QLabel*>("topicIdentity")->text();
    refresh->click(); assert(window.findChild<QLabel*>("topicIdentity")->text()==selected);
    assert(!start->isEnabled());
    assert(select("/legacy","PUBLISHER"));
    assert(!start->isEnabled() && status->text()=="Type descriptor unavailable");
    assert(select("/topic","PUBLISHER")); start->click(); assert(timer->isActive());
    fixture.terminate(); assert(fixture.waitForFinished(10000)&&fixture.exitCode()==0);
    wait([&]{return !timer->isActive();});
    assert(status->text().startsWith("Echo error:"));
    assert(window.statusBar()->currentMessage().startsWith("Echo error:"));
    refresh->click(); assert(connection->text()=="Connection: Disconnected");
    assert(topics->count()==0 && fields->rowCount()==0 && !start->isEnabled());
    // Unlink while the owner stays alive: no refresh and no stale mapped value.
    fixture.start(argv[1],{"--element",prefix,"--no-sample"}); assert(fixture.waitForStarted());
    wait([&]{refresh->click(); return select("/topic","PUBLISHER") && start->isEnabled() && topics->count()==3 && window.findChild<QListWidget*>("parametersList")->count()==2;});
    start->click(); assert(timer->isActive());
    assert(fields->rowCount()==0 && status->text().contains("waiting"));
    assert(::kill(static_cast<pid_t>(fixture.processId()),SIGUSR1)==0);
    wait([&]{return fields->rowCount()==7;});
    assert(::kill(static_cast<pid_t>(fixture.processId()),SIGUSR2)==0);
    wait([&]{return !timer->isActive();});
    assert(status->text().startsWith("Echo error:"));
    assert(fixture.state()==QProcess::Running);
    fixture.terminate(); assert(fixture.waitForFinished(10000)&&fixture.exitCode()==0);
    window.close(); return 0;
}
