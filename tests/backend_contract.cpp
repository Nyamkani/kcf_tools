#include "kcf_tool/backend/kcf_backend.hpp"
#include "kcf_tool/backend/mock_backend.hpp"
#include <cerrno>
#include <cstdlib>
#include <iostream>

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
int main() {
    using namespace kcf_tool;
    MockBackend mock;
    KcfBackend kcf;
    Check(mock.GetConnectionState() == BackendConnectionState::CONNECTED, "mock connected");
    Check(kcf.GetConnectionState() == BackendConnectionState::DISCONNECTED, "KCF disconnected");
    Check(kcf.GetApplicationInfo().name.empty() && kcf.GetApplicationInfo().state == "DISCONNECTED",
          "KCF has no fake application");
    Check(kcf.GetElements().empty() && kcf.GetTopics().empty() && kcf.GetParameters().empty(), "KCF empty discovery");
    TopicInfo topic{};
    ParameterInfo parameter{};
    const std::string topic_name = "/kcf_mecanum_motor_odometry";
    const std::string parameter_name = "/mecanum/motor/odom_period_ms";
    Check(mock.GetTopicInfo(topic_name, topic) == 0 && topic.type_name == "mecanum::OdometryData", "topic lookup");
    Check(mock.GetTopicInfo("missing", topic) == -ENOENT && topic.name == topic_name, "missing topic preserves output");
    Check(mock.SetParameter(parameter_name, "25") == 0, "mock set");
    Check(mock.GetParameter(parameter_name, parameter) == 0 && parameter.value == "25", "get after set");
    Check(mock.GetParameter("missing", parameter) == -ENOENT && parameter.name == parameter_name, "missing parameter preserves output");
    DataSnapshot snapshot{"sentinel", 42, 123, {{"x", "double", "1.0"}}};
    bool called = false;
    for (ToolBackend* backend : {static_cast<ToolBackend*>(&mock), static_cast<ToolBackend*>(&kcf)}) {
        if (backend == &kcf) Check(backend->ReadTopicLatest(topic_name, snapshot) == -ENOTSUP, "stub payload unsupported");
        Check(snapshot.type_name == "sentinel" && snapshot.sequence == 42 && snapshot.timestamp_us == 123 &&
              snapshot.fields.size() == 1 && snapshot.fields[0].value == "1.0", "snapshot unchanged");
        Check(backend->StartTopicMonitor(topic_name, [&](const DataSnapshot&) { called = true; }) == -ENOTSUP,
              "monitor unsupported");
        Check(backend->StartTopicMonitor(topic_name, {}) == -ENOTSUP, "empty callback unsupported");
        Check(backend->StopTopicMonitor(topic_name) == -ENOTSUP, "stop unsupported");
        backend->Refresh();
    }
    Check(mock.ReadTopicLatest(topic.identity, snapshot) == 0 && snapshot.type_id == topic.type_id,
          "mock identity payload");
    Check(snapshot.fields[2].array_values.size() == 4, "mock array payload");
    const auto sequence = snapshot.sequence;
    Check(mock.ReadTopicLatest(topic_name, snapshot) == 0 && snapshot.sequence > sequence,
          "mock named payload advances");
    Check(mock.ReadTopicLatest(EndpointIdentity{}, snapshot) == -ENOENT && snapshot.sequence > sequence,
          "invalid identity preserves output");
    const auto first=mock.GetTopics()[0].identity, second=mock.GetTopics()[1].identity;
    Check(mock.ReadTopicEcho(snapshot)==-EBADF, "inactive mock echo");
    Check(mock.StartTopicEcho(first)==0 && mock.ReadTopicEcho(snapshot)==0 && snapshot.fields[1].value=="1.25", "mock echo A");
    Check(mock.StartTopicEcho(second)==0 && mock.ReadTopicEcho(snapshot)==0 && snapshot.fields[1].value=="2.25", "mock switches to B");
    mock.StopTopicEcho();Check(mock.ReadTopicEcho(snapshot)==-EBADF, "mock stop");
    Check(mock.StartTopicEcho(first)==0, "mock restart");mock.Refresh();
    Check(mock.ReadTopicEcho(snapshot)==-EBADF, "mock refresh closes");
    Check(kcf.StartTopicEcho(first)==-ENOTSUP && kcf.ReadTopicEcho(snapshot)==-ENOTSUP, "stub echo unsupported");
    Check(!called, "unsupported monitor never invokes callback");
    Check(kcf.Refresh() == -ENOTSUP, "KCF refresh unsupported");
    Check(kcf.GetTopicInfo(topic_name, topic) == -ENOTSUP && topic.name == topic_name, "KCF topic unsupported");
    Check(kcf.GetParameter(parameter_name, parameter) == -ENOTSUP && parameter.value == "25", "KCF get unsupported");
    Check(kcf.SetParameter(parameter_name, "30") == -ENOTSUP, "KCF set unsupported");
    Check(kcf.GetConnectionState() == BackendConnectionState::DISCONNECTED, "KCF remains disconnected");
    std::cout << "PASS: Qt-independent backend contracts\n";
}
