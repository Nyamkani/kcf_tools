#pragma once
#include "kcf_tool/model/endpoint_identity.hpp"
#include <cstdint>
#include <string>

namespace kcf_tool {
struct ElementInfo {
    std::string name{};
    std::int32_t pid{};
    std::string executable{};
    std::string mode{};
    std::string state{};
    std::uint64_t heartbeat{};
    std::int32_t runtime_error{};
    RuntimeIdentity identity{};
    RuntimeIdentity supervisor{};
    std::string application_name{};
};
}
