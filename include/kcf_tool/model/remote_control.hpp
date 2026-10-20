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
    std::chrono::steady_clock::time_point observed_at{};
    DataSnapshot feedback,result;
};
struct RemoteReply {
    int error{0};bool response_received{false},execution_unknown{false},cancel_unknown{false},accepted{false};
    int readback_error{0};std::uint64_t handle{0};
    DataSnapshot value;std::vector<RemoteActivity> activities;
};
inline std::future<RemoteReply> RemoteUnavailable(int error=-ENOTSUP){std::promise<RemoteReply> p;auto f=p.get_future();RemoteReply r;r.error=error;p.set_value(r);return f;}
}
