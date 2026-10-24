#pragma once
#include "kcf_tool/model/network_info.hpp"
#include <memory>
namespace kcf_tool {
struct LocalGatewayEndpoint {
    RouteEndpointIdentity identity, application; // application.registration_id is unused
    std::string host_name, application_name, element_name, name;
    unsigned kind{}, role{}, scope{};
    RemoteTypeIdentity value_type, request_type, response_type, feedback_type;
    std::vector<std::string> groups;
};
struct LocalGatewayPeer {
    std::string host, boot, address, state;
    std::uint64_t session{}, metadata_revision{};
    unsigned metadata_port{};bool metadata_ready{};
    std::vector<LocalGatewayEndpoint> endpoints;
};
enum class LocalGatewayConnection { DISABLED, CONNECTING, READY, ERROR };
struct LocalGatewaySnapshot {
    LocalGatewayConnection connection{LocalGatewayConnection::DISABLED};
    std::string socket_path, state, host, boot, interface_name, ipv4;
    std::uint64_t session{}, observed_at_unix_ms{};
    unsigned interface_index{};int error{},last_network_error{};
    std::vector<LocalGatewayPeer> peers;
};
// Independent of Framework/Qt/Discovery. Single worker, 1 s query-cycle deadline,
// 1 s between cycles, max 1 MiB response. Stop cancels pending socket waits.
// Snapshot is copied. Failed cycles clear previous status/peer data.
class LocalGatewayClient {
public:
    explicit LocalGatewayClient(std::string path="/run/kcf/gateway.sock");
    ~LocalGatewayClient();
    LocalGatewayClient(const LocalGatewayClient&)=delete;
    LocalGatewayClient& operator=(const LocalGatewayClient&)=delete;
    void Stop(); // caller-serialized with destruction
    LocalGatewaySnapshot Snapshot() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
