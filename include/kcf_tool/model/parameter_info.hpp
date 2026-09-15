#pragma once
#include "kcf_tool/model/endpoint_identity.hpp"
#include "kcf_tool/model/dynamic_value.hpp"
#include <string>

namespace kcf_tool {
struct ParameterInfo {
    std::string name{};
    std::string type_name{};
    std::string value{};
    bool writable{};
    EndpointIdentity identity{};
    std::string role{};
    std::uint64_t payload_size{0},type_id{0};
    std::string diagnostic_type_name{};
    DataSnapshot snapshot{};
    bool text_editable{true};
};
}
