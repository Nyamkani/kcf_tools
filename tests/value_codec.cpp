#ifdef NDEBUG
#undef NDEBUG
#endif
#include "value_codec.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
struct Values {bool b;std::int8_t i8;std::uint8_t u8;std::int16_t i16;std::uint16_t u16;std::int32_t i32;std::uint32_t u32;std::int64_t i64;std::uint64_t u64;float f;double d;std::int16_t array[3];};
int main(){using namespace kcf_tool;
    auto d=kcf::MakeTypeDescriptor<Values>("tool.codec.Values.v1");d.field_count=12;
#define F(I,M,T) d.fields[I]=kcf::MakeField<Values,T>(#M,offsetof(Values,M));
    F(0,b,bool) F(1,i8,std::int8_t) F(2,u8,std::uint8_t) F(3,i16,std::int16_t) F(4,u16,std::uint16_t) F(5,i32,std::int32_t) F(6,u32,std::uint32_t) F(7,i64,std::int64_t) F(8,u64,std::uint64_t) F(9,f,float) F(10,d,double) F(11,array,std::int16_t[3])
#undef F
    Values v{true,-128,255,-32768,65535,INT32_MIN,UINT32_MAX,INT64_MIN,UINT64_MAX,1.0f/3.0f,0.1,{-1,0,32767}};
    kcf::DynamicPayload raw;raw.type_id=d.type_id;raw.sequence=19;raw.bytes.resize(sizeof(v));std::memcpy(raw.bytes.data(),&v,sizeof(v));
    DataSnapshot decoded;assert(detail::Decode(d,raw,decoded)==0&&decoded.sequence==19&&decoded.type_id==d.type_id&&decoded.timestamp_us==0);
    assert(decoded.fields[0].value=="true"&&decoded.fields[7].value=="-9223372036854775808"&&decoded.fields[8].value=="18446744073709551615");
    assert((decoded.fields[11].array_values==std::vector<std::string>{"-1","0","32767"}));
    kcf::DynamicPayload encoded;assert(detail::Encode(d,decoded,encoded,&raw)==0&&encoded.bytes==raw.bytes);
    auto sentinel=encoded;
    for(int mode=0;mode<11;++mode){auto bad=decoded;
        switch(mode){case 0:bad.type_id++;break;case 1:bad.type_name="wrong";break;case 2:bad.fields.pop_back();break;case 3:bad.fields[1].name="unknown";break;
        case 4:bad.fields[1].name="b";break;case 5:bad.fields[11].array_values.pop_back();break;case 6:bad.fields[1].value="128";break;case 7:bad.fields[2].value="-1";break;
        case 8:bad.fields[0].value="2";break;case 9:bad.fields[1].kind=ValueKind::FLOATING_POINT;break;case 10:bad.fields[1].type_name="int64";break;}
        assert(detail::Encode(d,bad,encoded)<0&&encoded.bytes==sentinel.bytes);
    }
    DataSnapshot saved=decoded;auto malformed=d;malformed.fields[11].offset=UINT32_MAX;
    assert(detail::Decode(malformed,raw,decoded)==-EPROTO&&decoded.fields[0].value==saved.fields[0].value);
    auto wrong=raw;wrong.bytes.pop_back();assert(detail::Decode(d,wrong,decoded)==-EMSGSIZE);
    wrong=raw;wrong.type_id++;assert(detail::Decode(d,wrong,decoded)==-EPROTOTYPE);
    wrong=raw;wrong.bytes[offsetof(Values,b)]=std::byte{2};assert(detail::Decode(d,wrong,decoded)==-EPROTO);
    // Packed layout is decoded via memcpy into aligned local primitives.
    auto packed=kcf::MakeTypeDescriptor<std::uint8_t[5]>("tool.codec.Packed.v1");packed.field_count=1;
    packed.fields[0]=kcf::MakeField<Values,std::uint32_t>("unaligned",1);
    kcf::DynamicPayload bytes;bytes.type_id=packed.type_id;bytes.bytes.resize(5);std::uint32_t number=123456;std::memcpy(bytes.bytes.data()+1,&number,4);
    assert(detail::Decode(packed,bytes,decoded)==0&&decoded.fields[0].value=="123456");
    std::cout<<"PASS codec: 11 primitives, precision/range, arrays, strict fields/type IDs, malformed buffers and unaligned layout\n";
}
