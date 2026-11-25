#pragma once
#include "kcf_tool/model/network_info.hpp"
#include "kcf_tool/model/dynamic_value.hpp"
#include <future>
#include <chrono>
#include <cerrno>
namespace kcf_tool {
enum class RemoteOperation { Prepare, Get, Set, Call, Goal, Cancel, Subscribe, Unsubscribe, Poll };
struct RemoteCommand {
    RemoteOperation operation{RemoteOperation::Prepare};
    RemoteEndpoint endpoint;
    DataSnapshot input;
    std::uint64_t handle{0};
    std::uint32_t timeout_ms{1000},tracking_ms{60000};
};
struct RemoteActivity {
    std::uint64_t handle{0};RemoteEndpoint endpoint;
    bool topic{false},accepted{false},terminal{false},communication_lost{false};
    bool execution_unknown{false},cancel_unknown{false},cancel_accepted{false};
    bool has_value{false};int state{0},error{0};
    std::uint64_t sequence{0},received_packets{0},route_id{0};
    // Exact receive-side route identity for cross-checking sender Gateway diagnostics.
    RouteEndpointIdentity route_source,route_destination;
    std::uint64_t source_session{0},destination_session{0};
    std::chrono::steady_clock::time_point observed_at{};
    DataSnapshot feedback,result;
};
struct RemoteReply {
    int error{0};bool response_received{false},execution_unknown{false},cancel_unknown{false},accepted{false};
    int readback_error{0};std::uint64_t handle{0};
    DataSnapshot value;std::vector<RemoteActivity> activities;
};
// Correlate a receive-side subscription with the *sender* Gateway's route.
// A Topic name, route ID or Host alone is never sufficient. Unknown/truncated,
// offline or stale diagnostics never establish a matching path.
inline const RemoteRoute* FindRemoteTopicSenderRoute(
    const NetworkSnapshot& snapshot,const RemoteActivity& activity,
    std::uint64_t now_unix_ms,std::uint64_t max_age_ms=10000) {
    if(!activity.topic||!activity.route_id||activity.communication_lost||activity.terminal||
       activity.source_session==0||activity.destination_session==0)return nullptr;
    const auto same=[](const RouteEndpointIdentity& a,const RouteEndpointIdentity& b){
        return a.host_id==b.host_id&&a.boot_id==b.boot_id&&a.pid==b.pid&&
               a.start_ticks==b.start_ticks&&a.registration_id==b.registration_id;
    };
    const RemoteRoute* matched=nullptr;
    for(const auto& host:snapshot.hosts){
        if(host.id!=activity.route_source.host_id||host.boot_id!=activity.route_source.boot_id||
           host.session!=activity.source_session||host.state!="ONLINE")continue;
        const auto& d=host.diagnostics;
        if((d.status!=DiagnosticsState::VALID&&d.status!=DiagnosticsState::TRUNCATED)||
           d.gateway_host!=host.id||d.gateway_boot!=host.boot_id||
           d.gateway_session!=host.session||!d.observed_at_unix_ms||
           d.observed_at_unix_ms>now_unix_ms||
           now_unix_ms-d.observed_at_unix_ms>max_age_ms)continue;
        for(const auto& r:d.routes){
            if(!r.sending||r.route_id!=activity.route_id||
               r.source_session!=activity.source_session||
               r.destination_session!=activity.destination_session||
               r.name!=activity.endpoint.name||
               !same(r.source,activity.route_source)||
               !same(r.destination,activity.route_destination))continue;
            if(matched)return nullptr; // Ambiguous: fail closed.
            matched=&r;
        }
    }
    return matched;
}
inline std::future<RemoteReply> RemoteUnavailable(int error=-ENOTSUP){std::promise<RemoteReply> p;auto f=p.get_future();RemoteReply r;r.error=error;p.set_value(r);return f;}
}
