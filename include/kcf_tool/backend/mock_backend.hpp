#pragma once
#include "kcf_tool/backend/tool_backend.hpp"

namespace kcf_tool {
class MockBackend final : public ToolBackend {
public:
    MockBackend();
    BackendConnectionState GetConnectionState() const override;
    ApplicationInfo GetApplicationInfo() override;
    std::vector<ElementInfo> GetElements() override;
    std::vector<TopicInfo> GetTopics() override;
    std::vector<ParameterInfo> GetParameters() override;
    int GetTopicInfo(const std::string& name, TopicInfo& topic) override;
    int ReadTopicLatest(const std::string& name, DataSnapshot& snapshot) override;
    int StartTopicMonitor(const std::string& topic_name, TopicDataCallback callback) override;
    int StopTopicMonitor(const std::string& topic_name) override;
    int GetParameter(const std::string& name, ParameterInfo& parameter) override;
    int SetParameter(const std::string& name, const std::string& value) override;
    int GetParameter(const EndpointIdentity&, DataSnapshot&) override;
    int SetParameter(const EndpointIdentity&, const DataSnapshot&) override;
    int ValidateParameter(const EndpointIdentity&, const DataSnapshot&) override;
    std::vector<ServiceInfo> QueryServices() override;
    int GetTypeTemplate(const RuntimeIdentity&, std::uint64_t, DataSnapshot&) override;
    int ValidateService(const EndpointIdentity&, const DataSnapshot&) override;
    int CallService(const EndpointIdentity&, const DataSnapshot&, DataSnapshot&,
                    std::uint32_t timeout_ms=200,std::uint32_t retry_count=2) override;
    int Refresh() override;
    int ReadTopicLatest(const EndpointIdentity&, DataSnapshot&) override;
    int StartTopicEcho(const EndpointIdentity&) override;
    int ReadTopicEcho(DataSnapshot&) override;
    void StopTopicEcho() override;
    int PrepareTopicRead(const EndpointIdentity&) override;
    void CloseTopicRead() override;
private:
    bool echo_active_{false};
    EndpointIdentity echo_identity_{};
    std::vector<ElementInfo> elements_;
    std::vector<TopicInfo> topics_;
    std::vector<ParameterInfo> parameters_;
};
}
