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
// VALID with an empty route list is a successful empty observation. TRUNCATED
// is a valid partial observation: omitted routes are unknown, not absent.
// Every other state has no usable route data. Timestamps describe observation,
// not a guarantee of current data; discovery freshness is rechecked each tick.
enum class DiagnosticsState { UNAVAILABLE, UNSUPPORTED, INCOMPATIBLE, PENDING, VALID, TRUNCATED, LOST, ERROR, TIMEOUT };
struct RouteEndpointIdentity {
    std::string host_id, boot_id;
    std::int32_t pid{};
    std::uint64_t start_ticks{}, registration_id{};
};
enum class NetworkRateState { INVALID, MEASURING, VALID };
struct NetworkCounterDelta {
    std::uint64_t packets_sent{}, packets_received{}, samples_published{}, dropped{}, duplicate{},
        out_of_order{}, missed_sequence{}, malformed{}, local_samples_skipped{};
};
struct NetworkRouteRates {
    NetworkRateState status{NetworkRateState::INVALID};
    double interval_seconds{}, send_hz{}, receive_hz{}, proxy_publish_hz{}, local_skipped_per_second{};
    NetworkCounterDelta delta; // valid only with VALID; same interval as the rates
};
struct RemoteRoute {
    RouteEndpointIdentity source, destination;
    std::uint64_t source_session{}, destination_session{}, route_id{};
    std::string name;
    std::uint8_t state{}, reason{}, proxy_recovery{}; // Framework enum values, no inferred state
    bool sending{};
    std::int32_t error{};
    std::uint64_t packets_sent{}, packets_received{}, samples_published{}, dropped{}, duplicate{},
        out_of_order{}, missed_sequence{}, malformed{}, local_samples_skipped{};
    NetworkRouteRates rates;
};
struct RemoteNetworkDiagnostics {
    DiagnosticsState status{DiagnosticsState::UNAVAILABLE};
    std::string gateway_host, gateway_boot;
    std::uint64_t gateway_session{}, observed_at_unix_ms{};
    std::int32_t error{};
    std::uint32_t total_routes{};
    bool truncated{};
    std::vector<RemoteRoute> routes; // per Gateway; never merged by name
};
struct RemoteHost {
    std::string id, boot_id, label, address, state, framework_version, network_version;
    std::string compatibility, capabilities;
    std::uint64_t session{}, revision{}, age_ms{};
    bool metadata_ready{};
    std::vector<RemoteEndpoint> endpoints;
    RemoteNetworkDiagnostics diagnostics;
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
