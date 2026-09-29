#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace kcf_tool {
// Display DTOs only, not a wire protocol. Keys include host + boot + runtime +
// registration; discovery session is separate freshness information.
struct RemoteEndpoint {
    std::string id, runtime_id, application_id;
    std::string application, element, name, kind, role, scope;
    std::string type, diagnostic;
};
struct RemoteHost {
    std::string id, boot_id, label, address, state, framework_version, network_version;
    std::string compatibility, capabilities;
    std::uint64_t session{}, revision{}, age_ms{};
    bool metadata_ready{};
    std::vector<RemoteEndpoint> endpoints;
};
struct NetworkSnapshot {
    std::string status{"Network discovery disabled / unavailable"};
    std::string route_status{"External Gateway route/statistics observation API unavailable; no inferred network edges"};
    std::vector<RemoteHost> hosts;
};
struct NetworkOptions {
    std::string interface_address{"0.0.0.0"};
    std::uint16_t port{37651};
};
}
