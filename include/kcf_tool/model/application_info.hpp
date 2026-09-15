#pragma once
#include "kcf_tool/model/endpoint_identity.hpp"
#include <string>

namespace kcf_tool {
struct ApplicationInfo {
    std::string name{};
    std::string state{};
    RuntimeIdentity supervisor{};
    std::uint64_t revision{0};
    std::uint32_t managed_element_count{0};
};
}
