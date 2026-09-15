#include "kcf_tool/backend/mock_backend.hpp"
#include <cerrno>
#include <charconv>
#include <cmath>
#include <limits>
#include <sstream>
#include <locale>

namespace kcf_tool {
MockBackend::MockBackend()
    : elements_{
          {"motor", 12001, "kcf_mecanum_motor", "SUPERVISED", "RUNNING", 0, 0},
          {"imu", 12002, "kcf_mecanum_imu", "SUPERVISED", "RUNNING", 0, 0},
          {"lidar", 12003, "kcf_mecanum_lidar", "SUPERVISED", "RUNNING", 0, 0}},
      topics_{
          {"/kcf_mecanum_motor_command", "mecanum::VelocityCommand", 24,
           {"navigation"}, {"motor"}, 50.0, 1000},
          {"/kcf_mecanum_motor_odometry", "mecanum::OdometryData", 64,
           {"motor"}, {"localization"}, 66.7, 10452},
          {"/kcf_mecanum_imu", "mecanum::ImuData", 48,
           {"imu"}, {}, 100.0, 2000}},
      parameters_{
          {"/mecanum/motor/wheel_radius_mm", "double", "41.0", true},
          {"/mecanum/motor/odom_period_ms", "uint32", "15", true},
          {"/mecanum/motor/serial_port", "string", "/dev/ttyUSB1", false}} {
    for(auto& e:elements_){e.identity={e.pid,1};e.supervisor={12000,1};e.application_name=GetApplicationInfo().name;}
    for(std::size_t i=0;i<topics_.size();++i){
        auto& t=topics_[i];t.identity={{12001+static_cast<std::int32_t>(i),1},i+1};
        t.role="PUBLISHER";t.type_id=0x4d4f434b0000ULL+i+1;
    }
    for(std::size_t i=0;i<parameters_.size();++i){
        auto& p=parameters_[i];p.identity={{12001,1},100+i};p.role="OWNER";
        p.diagnostic_type_name=p.type_name;p.type_id=i<2?0x4d4f434b1000ULL+i+1:0;
        p.payload_size=i==0?8:i==1?4:0;
    }

}

BackendConnectionState MockBackend::GetConnectionState() const {
    return BackendConnectionState::CONNECTED;
}
int MockBackend::GetTopicInfo(const std::string& name, TopicInfo& topic) {
    for (const auto& candidate : topics_) {
        if (candidate.name == name) { topic = candidate; return 0; }
    }
    return -ENOENT;
}
int MockBackend::GetParameter(const std::string& name, ParameterInfo& parameter) {
    for (const auto& candidate : parameters_) {
        if (candidate.name == name) { parameter = candidate; return 0; }
    }
    return -ENOENT;
}
// Deterministic local display samples; these IDs describe mock data, not KCF ABI.
int MockBackend::StartTopicEcho(const EndpointIdentity& id) {
    StopTopicEcho();
    for(const auto& t:topics_)if(t.identity==id){echo_identity_=id;echo_active_=true;return 0;}
    return -ENOENT;
}
int MockBackend::ReadTopicEcho(DataSnapshot& output) {
    if(!echo_active_)return -EBADF;
    return ReadTopicLatest(echo_identity_,output);
}
void MockBackend::StopTopicEcho(){echo_active_=false;echo_identity_={};}
int MockBackend::PrepareTopicRead(const EndpointIdentity& id){return StartTopicEcho(id);}
void MockBackend::CloseTopicRead(){StopTopicEcho();}
int MockBackend::ReadTopicLatest(const std::string& name, DataSnapshot& output) {
    for(const auto& t:topics_)if(t.name==name)return ReadTopicLatest(t.identity,output);
    return -ENOENT;
}
int MockBackend::ReadTopicLatest(const EndpointIdentity& id, DataSnapshot& output) {
    for(auto& t:topics_)if(t.identity==id){
        DataSnapshot value;value.type_id=t.type_id;value.type_name=t.type_name;value.sequence=++t.sequence;
        value.fields={{"sequence","uint64",std::to_string(value.sequence),ValueKind::UNSIGNED_INTEGER,{}},
                      {"x","float32",std::to_string(t.identity.registration_id)+".25",ValueKind::FLOATING_POINT,{}},
                      {"values","float32","",ValueKind::FLOATING_POINT,{"2","4","6","8"}}};
        output=std::move(value);return 0;
    }
    return -ENOENT;
}
int MockBackend::StartTopicMonitor(const std::string&, TopicDataCallback) { return -ENOTSUP; }
int MockBackend::StopTopicMonitor(const std::string&) { return -ENOTSUP; }

ApplicationInfo MockBackend::GetApplicationInfo() { return {"mecanum", "RUNNING", {12000,1}, 1, 3}; }
std::vector<ElementInfo> MockBackend::GetElements() { return elements_; }
std::vector<TopicInfo> MockBackend::GetTopics() { return topics_; }
std::vector<ParameterInfo> MockBackend::GetParameters() { return parameters_; }

int MockBackend::GetParameter(const EndpointIdentity& id,DataSnapshot& out){
    for(const auto& p:parameters_)if(p.identity==id){
        if(!p.type_id)return -ENOTSUP;
        DataSnapshot value;value.type_id=p.type_id;value.type_name=p.type_name;
        value.fields={{"value",p.type_name=="double"?"float64":"uint32",p.value,
            p.type_name=="double"?ValueKind::FLOATING_POINT:ValueKind::UNSIGNED_INTEGER,{}}};
        out=std::move(value);return 0;
    }return -ENOENT;
}
int MockBackend::ValidateParameter(const EndpointIdentity& id,const DataSnapshot& value){
    for(const auto& p:parameters_)if(p.identity==id){
        if(!p.writable)return -EPERM;
        if(!p.type_id)return -ENOTSUP;
        DataSnapshot schema;int r=GetParameter(id,schema);if(r)return r;
        if(value.type_id!=schema.type_id||value.type_name!=schema.type_name)return -EPROTOTYPE;
        if(value.fields.size()!=1)return -EINVAL;
        const auto& f=value.fields[0];const auto& expected=schema.fields[0];
        if(f.name!=expected.name)return -EINVAL;
        if(f.type_name!=expected.type_name||f.kind!=expected.kind)return -EPROTOTYPE;
        if(!f.array_values.empty())return -EMSGSIZE;
        if(f.type_name=="uint32"){
            std::uint64_t n{};auto parsed=std::from_chars(f.value.data(),f.value.data()+f.value.size(),n);
            if(parsed.ec==std::errc::result_out_of_range||n>UINT32_MAX)return -ERANGE;
            if(parsed.ec!=std::errc{}||parsed.ptr!=f.value.data()+f.value.size())return -EINVAL;
        }else{
            double n{};std::istringstream in(f.value);in.imbue(std::locale::classic());in>>std::noskipws>>n;
            if(!in||!in.eof()||!std::isfinite(n))return -EINVAL;
        }return 0;
    }return -ENOENT;
}
int MockBackend::SetParameter(const EndpointIdentity& id,const DataSnapshot& value){
    int r=ValidateParameter(id,value);if(r)return r;
    for(auto& p:parameters_)if(p.identity==id){p.value=value.fields[0].value;return 0;}
    return -ENOENT;
}
int MockBackend::SetParameter(const std::string& name, const std::string& value) {
    for (auto& parameter : parameters_) {
        if (parameter.name != name) continue;
        if (!parameter.writable) return -EPERM;
        parameter.value = value;
        return 0;
    }
    return -ENOENT;
}

std::vector<ServiceInfo> MockBackend::QueryServices(){
    std::vector<ServiceInfo> result;
    for(unsigned i=0;i<2;++i){ServiceInfo s;s.identity={{12001,1},900+i};s.name=i?"/mock/subtract":"/mock/add";
        s.service_id=i+1;s.request_type_id=0x6d0101;s.response_type_id=0x6d0102;s.request_size=8;s.response_size=4;
        s.diagnostic_request_type_name="mock.AddRequest";s.diagnostic_response_type_name="mock.AddResponse";result.push_back(s);}
    return result;
}
int MockBackend::GetTypeTemplate(const RuntimeIdentity& id,std::uint64_t type,DataSnapshot& output){
    if(!(id==RuntimeIdentity{12001,1}))return -ENOENT;
    DataSnapshot value;value.type_id=type;
    if(type==0x6d0101){value.type_name="mock.AddRequest";value.fields={{"a","int32","0",ValueKind::SIGNED_INTEGER,{}},{"b","int32","0",ValueKind::SIGNED_INTEGER,{}}};}
    else if(type==0x6d0102){value.type_name="mock.AddResponse";value.fields={{"result","int32","0",ValueKind::SIGNED_INTEGER,{}}};}
    else return -ENOENT;
    output=std::move(value);return 0;
}
int MockBackend::ValidateService(const EndpointIdentity& id,const DataSnapshot& request){
    bool found=false;for(const auto& s:QueryServices())if(s.identity==id)found=true;
    if(!found)return -ENOENT;
    if(request.type_id!=0x6d0101||request.type_name!="mock.AddRequest")return -EPROTOTYPE;
    if(request.fields.size()!=2)return -EINVAL;
    bool used[2]={false,false};
    for(const auto& f:request.fields){const int i=f.name=="a"?0:f.name=="b"?1:-1;if(i<0||used[i])return -EINVAL;used[i]=true;
        if(f.kind!=ValueKind::SIGNED_INTEGER||f.type_name!="int32")return -EPROTOTYPE;
        if(!f.array_values.empty())return -EMSGSIZE;
        std::int64_t n{};const auto parsed=std::from_chars(f.value.data(),f.value.data()+f.value.size(),n);
        if(parsed.ec==std::errc::result_out_of_range||n<INT32_MIN||n>INT32_MAX)return -ERANGE;
        if(parsed.ec!=std::errc{}||parsed.ptr!=f.value.data()+f.value.size())return -EINVAL;
    }return 0;
}
int MockBackend::CallService(const EndpointIdentity& id,const DataSnapshot& request,DataSnapshot& output,std::uint32_t,std::uint32_t){
    const int r=ValidateService(id,request);if(r)return r;
    std::int64_t a=0,b=0;for(const auto& f:request.fields){auto& n=f.name=="a"?a:b;std::from_chars(f.value.data(),f.value.data()+f.value.size(),n);}
    const auto sum=id.registration_id==900?a+b:a-b;if(sum<INT32_MIN||sum>INT32_MAX)return -ERANGE;
    DataSnapshot response;GetTypeTemplate(id.runtime,0x6d0102,response);response.fields[0].value=std::to_string(sum);output=std::move(response);return 0;
}
int MockBackend::Refresh() {
    StopTopicEcho();
    for (auto& element : elements_) {
        if (element.state == "RUNNING") ++element.heartbeat;
    }
    return 0;
}
}
