#pragma once
#include "kcf_tool/model/endpoint_identity.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kcf_tool {
struct TopicInfo {
    std::string name{};
    std::string type_name{};
    std::size_t payload_size{};
    std::vector<std::string> publishers{};
    std::vector<std::string> subscribers{};
    double frequency_hz{};
    std::uint64_t sequence{};
    EndpointIdentity identity{};
    std::string role{};
    std::uint64_t type_id{0};
    std::string diagnostic_type_name{};
};
}
