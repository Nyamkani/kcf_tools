#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace kcf_tool {
// Tool values contain no framework PODs. Scalars use value; fixed arrays use
// array_values and leave value empty. type_name identifies the exact width.
enum class ValueKind {
    BOOLEAN,
    SIGNED_INTEGER,
    UNSIGNED_INTEGER,
    FLOATING_POINT,
    STRING,
    STRUCT
};

struct FieldValue {
    std::string name{};
    std::string type_name{};
    std::string value{};
    ValueKind kind{ValueKind::STRING};
    std::vector<std::string> array_values{};
};

struct DataSnapshot {
    std::string type_name{};
    std::uint64_t sequence = 0;
    // Microseconds; clock origin must be documented by a future backend.
    // Zero denotes an unavailable timestamp. No cross-backend comparison implied.
    std::uint64_t timestamp_us = 0;
    std::vector<FieldValue> fields{};
    std::uint64_t type_id{0};
};
}
