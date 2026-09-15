#pragma once
#include "kcf_tool/model/application_info.hpp"
#include "kcf_tool/model/element_info.hpp"
#include "kcf_tool/model/topic_info.hpp"
#include "kcf_tool/model/parameter_info.hpp"
#include "kcf_tool/model/backend_connection_state.hpp"
#include "kcf_tool/model/dynamic_value.hpp"
#include "kcf_tool/model/service_info.hpp"
#include <cerrno>
#include <functional>
#include <vector>

namespace kcf_tool {
// The snapshot reference is valid only during the callback; copy it to retain it.
// Callback thread affinity is unspecified. GUI consumers must use Qt queued
// invocation with a lifetime-safe receiver, never update widgets directly.
// Scheduling, stop/in-flight callback and destruction synchronization policies
// must be settled before a backend implements asynchronous monitoring.
using TopicDataCallback = std::function<void(const DataSnapshot&)>;

class ToolBackend {
public:
    virtual ~ToolBackend() = default;
    virtual BackendConnectionState GetConnectionState() const = 0;
    // Value-returning discovery APIs have no error channel: inspect connection
    // state and Refresh's result. Disconnected backends return empty lists.
    virtual ApplicationInfo GetApplicationInfo() = 0;
    virtual std::vector<ElementInfo> GetElements() = 0;
    virtual std::vector<TopicInfo> GetTopics() = 0;
    virtual std::vector<ParameterInfo> GetParameters() = 0;
    // int operations return 0 or negative errno. Failed output-parameter
    // operations leave the caller's output unchanged. Unsupported = -ENOTSUP.
    virtual int GetTopicInfo(const std::string& name, TopicInfo& topic) = 0;
    virtual int ReadTopicLatest(const std::string& name, DataSnapshot& snapshot) = 0;
    virtual int StartTopicMonitor(const std::string& topic_name, TopicDataCallback callback) = 0;
    virtual int StopTopicMonitor(const std::string& topic_name) = 0;
    virtual int GetParameter(const std::string& name, ParameterInfo& parameter) = 0;
    virtual int SetParameter(const std::string& name, const std::string& value) = 0;
    virtual int Refresh() = 0;
    // Manual, serialized backend operations. IDs preserve per-runtime endpoints.
    virtual std::vector<ApplicationInfo> GetApplications() { return {GetApplicationInfo()}; }
    // One persistent echo session; EAGAIN means no consistent sample yet.
    // Other read errors end the session. One-shot ReadTopicLatest stays separate.
    virtual int StartTopicEcho(const EndpointIdentity&) { return -ENOTSUP; }
    virtual int ReadTopicEcho(DataSnapshot&) { return -ENOTSUP; }
    virtual void StopTopicEcho() {}
    // Compatibility aliases for the earlier prepare/close contract.
    // Use ReadTopicEcho for polling; ReadTopicLatest is always one-shot.
    // Refresh/Close ends the session. No worker or automatic reconnect.
    virtual int PrepareTopicRead(const EndpointIdentity&) { return -ENOTSUP; }
    virtual void CloseTopicRead() {}
    virtual int ReadTopicLatest(const EndpointIdentity&, DataSnapshot&) { return -ENOTSUP; }
    virtual int GetParameter(const EndpointIdentity&, DataSnapshot&) { return -ENOTSUP; }
    // Validate the complete candidate without opening/writing parameter storage.
    virtual int ValidateParameter(const EndpointIdentity&, const DataSnapshot&) { return -ENOTSUP; }
    virtual int SetParameter(const EndpointIdentity&, const DataSnapshot&) { return -ENOTSUP; }
    // Zero-initialized field template, NOT a live value; no KCF POD exposure.
    virtual int GetTypeTemplate(const RuntimeIdentity&, std::uint64_t, DataSnapshot&) { return -ENOTSUP; }
    virtual std::vector<ServiceInfo> QueryServices() { return {}; }
    // Validate request using the same descriptor/encode rules as Call, without sending.
    virtual int ValidateService(const EndpointIdentity&, const DataSnapshot&) { return -ENOTSUP; }
    // A timeout does not prove the callback did not execute. No extra Tool retry.
    virtual int CallService(const EndpointIdentity&, const DataSnapshot&, DataSnapshot&,
                            std::uint32_t = 200, std::uint32_t = 2) { return -ENOTSUP; }
};
}
