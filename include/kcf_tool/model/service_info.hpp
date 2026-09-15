#pragma once
#include "kcf_tool/model/endpoint_identity.hpp"
#include <string>
namespace kcf_tool {
struct ServiceInfo {
    EndpointIdentity identity{};
    std::string name{};
    std::uint16_t port{0},service_id{0};
    std::uint32_t request_size{0},response_size{0};
    std::uint64_t request_type_id{0},response_type_id{0};
    std::string diagnostic_request_type_name,diagnostic_response_type_name;
};
}
