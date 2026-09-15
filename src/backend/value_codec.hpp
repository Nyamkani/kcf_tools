#pragma once
#include "kcf_tool/model/dynamic_value.hpp"
#include "kcf/introspection/type_descriptor.hpp"
#include "kcf/dynamic/dynamic_payload.hpp"
namespace kcf_tool::detail {
int Decode(const kcf::TypeDescriptor&,const kcf::DynamicPayload&,DataSnapshot&);
// Input must contain exactly the declared fields. Seed preserves non-field bytes
// for Parameter Set; service requests are initialized to zero.
int Encode(const kcf::TypeDescriptor&,const DataSnapshot&,kcf::DynamicPayload&,
           const kcf::DynamicPayload* seed=nullptr);
}
