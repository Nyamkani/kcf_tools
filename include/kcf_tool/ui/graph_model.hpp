#pragma once
#include "kcf_tool/model/network_info.hpp"
#include "kcf_tool/model/application_info.hpp"
#include "kcf_tool/model/element_info.hpp"
#include "kcf_tool/model/topic_info.hpp"
#include "kcf_tool/model/parameter_info.hpp"
#include "kcf_tool/model/service_info.hpp"
#include <vector>

namespace kcf_tool {
enum class GraphKind { Element, Topic, Parameter, Service, Action, Host };
struct GraphGroup { std::string id, label, host; };
struct GraphEndpoint { EndpointIdentity identity; std::string role; };
struct GraphNode {
    std::string id, label, group;
    GraphKind kind{GraphKind::Element};
    RuntimeIdentity runtime;
    std::vector<GraphEndpoint> endpoints;
    std::string host, detail; // Nonempty host means REMOTE; never activate Local APIs.
    bool lost{false};
};
struct GraphEdge { std::string from, to, role; EndpointIdentity endpoint; bool network_route{false}; std::string detail; };
struct GraphOptions { bool topics{true}, parameters{false}, services{false}; std::string group; bool actions{false}, local{true}, remote{true}; std::string host; };
struct GraphModel { std::vector<GraphGroup> groups; std::vector<GraphNode> nodes; std::vector<GraphEdge> edges; };
GraphModel BuildGraph(const std::vector<ApplicationInfo>& applications,
                      const std::vector<ElementInfo>& elements,
                      const std::vector<TopicInfo>& topics,
                      const std::vector<ParameterInfo>& parameters,
                      const std::vector<ServiceInfo>& services,
                      const GraphOptions& options = {});
// Local registration evidence only, never proof of delivery or a verified
// Network Route. Each edge retains both registrations, including incarnation.
enum class CommunicationEvidence { Metadata };
struct CommunicationEdge {
    std::string from, to, topic;
    EndpointIdentity publisher, subscriber;
    CommunicationEvidence evidence{CommunicationEvidence::Metadata};
};
struct RemoteCommunicationEdge {
    GraphEdge route; // Verified route mapped to runtime nodes, never Metadata.
    std::string topic;
    RemoteEndpoint publisher, subscriber; // full endpoint identities and sessions
};
struct CommunicationGraphModel {
    std::vector<GraphGroup> groups;
    std::vector<GraphNode> nodes; // Element nodes plus optional Remote Host headings; same IDs as Ownership.
    std::vector<CommunicationEdge> edges;
    std::vector<RemoteCommunicationEdge> network_edges;
};
CommunicationGraphModel BuildCommunicationGraph(
    const std::vector<ApplicationInfo>& applications,
    const std::vector<ElementInfo>& elements,
    const std::vector<TopicInfo>& topics,
    const GraphOptions& options = {});
CommunicationGraphModel AddRemoteCommunicationGraph(CommunicationGraphModel, const NetworkSnapshot&, const GraphOptions& = {});
GraphModel AddRemoteGraph(GraphModel local, const NetworkSnapshot&, const GraphOptions& = {});
}
