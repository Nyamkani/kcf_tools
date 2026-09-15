#include "kcf_tool/backend/kcf_backend.hpp"
#include <cerrno>

namespace kcf_tool {
struct KcfBackend::Impl {};
KcfBackend::KcfBackend()=default;
KcfBackend::~KcfBackend()=default;
BackendConnectionState KcfBackend::GetConnectionState() const {
    return BackendConnectionState::DISCONNECTED;
}
ApplicationInfo KcfBackend::GetApplicationInfo() { return {"", "DISCONNECTED"}; }
std::vector<ElementInfo> KcfBackend::GetElements() { return {}; }
std::vector<TopicInfo> KcfBackend::GetTopics() { return {}; }
std::vector<ParameterInfo> KcfBackend::GetParameters() { return {}; }
int KcfBackend::GetTopicInfo(const std::string&, TopicInfo&) { return -ENOTSUP; }
int KcfBackend::ReadTopicLatest(const std::string&, DataSnapshot&) { return -ENOTSUP; }
int KcfBackend::StartTopicMonitor(const std::string&, TopicDataCallback) { return -ENOTSUP; }
int KcfBackend::StopTopicMonitor(const std::string&) { return -ENOTSUP; }
int KcfBackend::GetParameter(const std::string&, ParameterInfo&) { return -ENOTSUP; }
int KcfBackend::SetParameter(const std::string&, const std::string&) { return -ENOTSUP; }
int KcfBackend::StartTopicEcho(const EndpointIdentity&){return -ENOTSUP;}
int KcfBackend::ReadTopicEcho(DataSnapshot&){return -ENOTSUP;}
void KcfBackend::StopTopicEcho(){}
int KcfBackend::PrepareTopicRead(const EndpointIdentity&){return -ENOTSUP;}
void KcfBackend::CloseTopicRead(){}
int KcfBackend::ValidateParameter(const EndpointIdentity&,const DataSnapshot&){return -ENOTSUP;}
int KcfBackend::Refresh() { return -ENOTSUP; }
std::vector<ApplicationInfo> KcfBackend::GetApplications(){return {};}
int KcfBackend::GetTypeTemplate(const RuntimeIdentity&,std::uint64_t,DataSnapshot&){return -ENOTSUP;}
std::vector<ServiceInfo> KcfBackend::QueryServices(){return {};}
int KcfBackend::ReadTopicLatest(const EndpointIdentity&,DataSnapshot&){return -ENOTSUP;}
int KcfBackend::GetParameter(const EndpointIdentity&,DataSnapshot&){return -ENOTSUP;}
int KcfBackend::SetParameter(const EndpointIdentity&,const DataSnapshot&){return -ENOTSUP;}
int KcfBackend::ValidateService(const EndpointIdentity&,const DataSnapshot&){return -ENOTSUP;}
int KcfBackend::CallService(const EndpointIdentity&,const DataSnapshot&,DataSnapshot&,std::uint32_t,std::uint32_t){return -ENOTSUP;}
}
