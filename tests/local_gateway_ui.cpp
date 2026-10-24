// Reuse the bounded protocol probe; its standalone test remains GUI-free.
#define main LocalGatewayProbeTestMain
#include "local_gateway.cpp"
#undef main
#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf_tool/ui/main_window.hpp"
#include <QApplication>
#include <QLabel>
#include <QTreeWidget>
#include <QPushButton>
template<class F>void UiWait(F f){auto end=std::chrono::steady_clock::now()+5s;while(!f()){assert(std::chrono::steady_clock::now()<end);QApplication::processEvents();std::this_thread::sleep_for(10ms);}}
int main(int argc,char** argv){QApplication app(argc,argv);char tmp[]="/tmp/tool-local-ui-XXXXXX";assert(mkdtemp(tmp));std::string path=std::string(tmp)+"/gateway.sock";
 auto backend=std::make_unique<KcfBackend>();assert(!backend->ConfigureLocalGateway(path,true));MainWindow window(std::move(backend));window.show();
 auto* status=window.findChild<QLabel*>("localGatewayStatus");auto* tree=window.findChild<QTreeWidget*>("localGatewayPeers");assert(status&&tree);
 UiWait([&]{return status->text().contains("ERROR");});
 {Probe server(path);for(int i=0;i<4;++i){server.state=i;const char* states[]={"RUNNING","WAITING_NETWORK","REBINDING","ERROR"};UiWait([&]{return status->text().contains("CONNECTED")&&status->text().contains(states[i]);});}
 UiWait([&]{return tree->topLevelItemCount()==1&&tree->topLevelItem(0)->childCount()==1;});
 for(auto* button:window.findChildren<QPushButton*>())if(button->text()=="Get / Revert"||button->text()=="Set + Readback"||button->text()=="Call once"||button->text()=="Start Goal")assert(!button->isEnabled());
 }
 UiWait([&]{return status->text().contains("IPC: ERROR")&&tree->topLevelItemCount()==0;});window.close();UiWait([&]{return !window.isVisible();});std::filesystem::remove_all(tmp);
 std::cout<<"Local Gateway UI states, peer separation, no Control grant, failed snapshot cleared PASS\n";
}
