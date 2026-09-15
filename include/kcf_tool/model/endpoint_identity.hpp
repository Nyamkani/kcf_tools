#pragma once
#include <cstdint>
namespace kcf_tool {
struct RuntimeIdentity {std::int32_t pid{0};std::uint64_t process_start_ticks{0};};
inline bool operator==(const RuntimeIdentity& a,const RuntimeIdentity& b){return a.pid==b.pid&&a.process_start_ticks==b.process_start_ticks;}
struct EndpointIdentity {RuntimeIdentity runtime;std::uint64_t registration_id{0};};
inline bool operator==(const EndpointIdentity& a,const EndpointIdentity& b){return a.runtime==b.runtime&&a.registration_id==b.registration_id;}
}
