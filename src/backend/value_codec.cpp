#include "value_codec.hpp"
#include <charconv>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
namespace kcf_tool::detail {
namespace {
const char* Name(kcf::FieldValueKind k){static const char* names[]={"","bool","int8","uint8","int16","uint16","int32","uint32","int64","uint64","float32","float64"};return names[static_cast<unsigned>(k)];}
ValueKind Kind(kcf::FieldValueKind k){auto n=static_cast<unsigned>(k);if(n==1)return ValueKind::BOOLEAN;if(n>=10)return ValueKind::FLOATING_POINT;return n%2==0?ValueKind::SIGNED_INTEGER:ValueKind::UNSIGNED_INTEGER;}
template<class T> std::string Read(const std::byte* bytes){T v;std::memcpy(&v,bytes,sizeof(v));std::ostringstream out;out.imbue(std::locale::classic());
    if constexpr(std::is_floating_point_v<T>)out<<std::setprecision(std::numeric_limits<T>::max_digits10)<<v;
    else if constexpr(std::is_signed_v<T>)out<<static_cast<std::int64_t>(v);else out<<static_cast<std::uint64_t>(v);
    return out.str();}
template<class T> int Write(const std::string& text,std::byte* bytes){T value{};if(text.empty())return -EINVAL;
    if constexpr(std::is_floating_point_v<T>){
        std::istringstream in(text);in.imbue(std::locale::classic());in>>std::noskipws>>value;
        if(!in||!in.eof()||!std::isfinite(value))return -ERANGE;
    }else{
        using Wide=std::conditional_t<std::is_signed_v<T>,std::int64_t,std::uint64_t>;Wide n{};
        const auto r=std::from_chars(text.data(),text.data()+text.size(),n);
        if(r.ec==std::errc::result_out_of_range)return -ERANGE;
        if(r.ec!=std::errc{}||r.ptr!=text.data()+text.size())return -EINVAL;
        if(n<std::numeric_limits<T>::min()||n>std::numeric_limits<T>::max())return -ERANGE;
        value=static_cast<T>(n);
    }
    std::memcpy(bytes,&value,sizeof(value));return 0;
}
#define KT_PRIMITIVES(F) F(INT8,std::int8_t) F(UINT8,std::uint8_t) F(INT16,std::int16_t) F(UINT16,std::uint16_t) F(INT32,std::int32_t) F(UINT32,std::uint32_t) F(INT64,std::int64_t) F(UINT64,std::uint64_t) F(FLOAT32,float) F(FLOAT64,double)
int ReadScalar(kcf::FieldValueKind k,const std::byte* p,std::string& text){switch(k){
case kcf::FieldValueKind::BOOL:{std::uint8_t v;std::memcpy(&v,p,1);if(v>1)return -EPROTO;text=v?"true":"false";return 0;}
#define CASE(K,T) case kcf::FieldValueKind::K:text=Read<T>(p);return 0;
KT_PRIMITIVES(CASE)
#undef CASE
}return -ENOTSUP;}
int WriteScalar(kcf::FieldValueKind k,const std::string& text,std::byte* p){switch(k){
case kcf::FieldValueKind::BOOL:{if(text!="true"&&text!="false")return -EINVAL;std::uint8_t v=text=="true";std::memcpy(p,&v,1);return 0;}
#define CASE(K,T) case kcf::FieldValueKind::K:return Write<T>(text,p);
KT_PRIMITIVES(CASE)
#undef CASE
}return -ENOTSUP;}
#undef KT_PRIMITIVES
bool Bounds(const kcf::TypeFieldInfo& f,std::size_t size){return f.offset<=size&&std::uint64_t(f.element_size)*f.array_count<=size-f.offset;}
}
int Decode(const kcf::TypeDescriptor& d,const kcf::DynamicPayload& p,DataSnapshot& output){
    if(!kcf::ValidateTypeDescriptor(d))return -EPROTO;
    if(p.type_id!=d.type_id)return -EPROTOTYPE;
    if(p.bytes.size()!=d.payload_size)return -EMSGSIZE;
    DataSnapshot value;value.type_name=d.type_name;value.type_id=d.type_id;value.sequence=p.sequence;
    for(std::uint32_t i=0;i<d.field_count;++i){const auto& f=d.fields[i];if(!Bounds(f,p.bytes.size()))return -EPROTO;
        FieldValue field;field.name=f.name;field.type_name=Name(f.kind);field.kind=Kind(f.kind);
        for(std::uint32_t j=0;j<f.array_count;++j){std::string text;int result=ReadScalar(f.kind,p.bytes.data()+f.offset+j*f.element_size,text);if(result)return result;
            if(f.array_count==1)field.value=std::move(text);else field.array_values.push_back(std::move(text));}
        value.fields.push_back(std::move(field));}
    output=std::move(value);return 0;
}
int Encode(const kcf::TypeDescriptor& d,const DataSnapshot& input,kcf::DynamicPayload& output,const kcf::DynamicPayload* seed){
    if(!kcf::ValidateTypeDescriptor(d))return -EPROTO;
    if(input.type_id!=d.type_id||input.type_name!=d.type_name)return -EPROTOTYPE;
    if(input.fields.size()!=d.field_count)return -EINVAL;
    if(seed&&(seed->type_id!=d.type_id||seed->bytes.size()!=d.payload_size))return -EPROTOTYPE;
    kcf::DynamicPayload value;if(seed)value=*seed;else value.bytes.resize(d.payload_size);value.type_id=d.type_id;
    std::vector<bool> used(d.field_count,false);
    for(const auto& field:input.fields){std::uint32_t i=0;while(i<d.field_count&&field.name!=d.fields[i].name)++i;
        if(i==d.field_count||used[i])return -EINVAL;
        used[i]=true;const auto& f=d.fields[i];
        if(!Bounds(f,value.bytes.size()))return -EPROTO;
        if(field.kind!=Kind(f.kind)||field.type_name!=Name(f.kind))return -EPROTOTYPE;
        if((f.array_count>1&&(field.array_values.size()!=f.array_count||!field.value.empty()))||(f.array_count==1&&!field.array_values.empty()))return -EMSGSIZE;
        for(std::uint32_t j=0;j<f.array_count;++j){int result=WriteScalar(f.kind,f.array_count==1?field.value:field.array_values[j],value.bytes.data()+f.offset+j*f.element_size);if(result)return result;}
    }
    output=std::move(value);return 0;
}
}
