#include "kcf_tool/backend/local_gateway.hpp"
#include <nlohmann/json.hpp>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <limits>
#include <set>
namespace kcf_tool {
namespace {
using Json=nlohmann::json;using Clock=std::chrono::steady_clock;
constexpr unsigned MAGIC=0x4b43464c,MAX_BODY=1024*1024;
struct Failure{int code;};
void Require(bool b){if(!b)throw Failure{-EPROTO};}
const Json& Field(const Json& j,const char* key){Require(j.is_object()&&j.contains(key));return j.at(key);}
std::string String(const Json& j){Require(j.is_string());return j.get<std::string>();}
std::uint64_t Decimal(const Json& j){const auto s=String(j);Require(!s.empty()&&s.size()<=20);std::uint64_t v;auto r=std::from_chars(s.data(),s.data()+s.size(),v);Require(r.ec==std::errc{}&&r.ptr==s.data()+s.size());return v;}
std::int64_t Integer(const Json& j,std::int64_t low,std::int64_t high){Require(j.is_number_integer());if(j.is_number_unsigned())Require(j.get<std::uint64_t>()<=std::uint64_t(high));auto v=j.get<std::int64_t>();Require(v>=low&&v<=high);return v;}
std::string Id(const Json& j,bool zero=false){auto s=String(j);Require(s.size()==32);bool nonzero=false;for(auto& c:s){Require((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'));if(c>='A'&&c<='F')c+=32;nonzero|=c!='0';}Require(zero||nonzero);return s;}
// Core v1 escapes each metadata byte as U+00XX, not UTF-8 codepoints.
std::string Text(const Json& j){auto s=String(j);Require(s.size()<=8192);std::string bytes;for(std::size_t i=0;i<s.size();++i){unsigned char c=s[i];if(c<128)bytes+=char(c);else{Require((c==0xc2||c==0xc3)&&i+1<s.size());unsigned char next=s[++i];Require(next>=0x80&&next<=0xbf);bytes+=char(((c&3)<<6)|(next&63));}}return bytes;}
bool Boolean(const Json& j){Require(j.is_boolean());return j.get<bool>();}
void Array(const Json& j,std::size_t limit){Require(j.is_array()&&j.size()<=limit);}
RouteEndpointIdentity Runtime(const Json& j,bool application=false){RouteEndpointIdentity r;r.host_id=Id(Field(j,"host"),application);r.boot_id=Id(Field(j,"boot"),application);r.pid=Integer(Field(j,"pid"),application?0:1,INT32_MAX);r.start_ticks=Decimal(Field(j,"start_ticks"));if(!r.pid)Require(r.host_id==std::string(32,'0')&&r.boot_id==std::string(32,'0')&&!r.start_ticks);else Require(r.start_ticks&&r.host_id!=std::string(32,'0')&&r.boot_id!=std::string(32,'0'));return r;}
RemoteTypeIdentity Type(const Json& j){RemoteTypeIdentity t;t.type_id=Decimal(Field(j,"type_id"));t.schema_id=Decimal(Field(j,"schema_id"));t.encoding_id=Integer(Field(j,"encoding_id"),0,UINT32_MAX);t.payload_size=Integer(Field(j,"payload_size"),0,UINT32_MAX);return t;}
void Status(const Json& j,LocalGatewaySnapshot& s){s.state=String(Field(j,"state"));Require(s.state=="WAITING_NETWORK"||s.state=="REBINDING"||s.state=="RUNNING"||s.state=="ERROR");s.host=Id(Field(j,"host"));s.boot=Id(Field(j,"boot"));s.session=Decimal(Field(j,"session"));s.interface_name=Text(Field(j,"interface"));s.ipv4=String(Field(j,"ipv4"));s.interface_index=Integer(Field(j,"interface_index"),0,UINT32_MAX);s.last_network_error=Integer(Field(j,"last_error"),INT32_MIN,0);in_addr ip{};Require(s.ipv4.empty()||inet_pton(AF_INET,s.ipv4.c_str(),&ip)==1);if(s.state=="RUNNING")Require(s.session&&!s.interface_name.empty()&&!s.ipv4.empty()&&s.interface_index);}
std::vector<LocalGatewayPeer> Peers(const Json& j){const auto& list=Field(j,"peers");Array(list,64);std::vector<LocalGatewayPeer> out;std::set<std::string> hosts;
 for(const auto& p:list){LocalGatewayPeer v;v.host=Id(Field(p,"host"));Require(hosts.insert(v.host).second);v.boot=Id(Field(p,"boot"));v.session=Decimal(Field(p,"session"));Require(v.session);v.state=String(Field(p,"state"));Require(v.state=="ONLINE"||v.state=="LOST");v.address=String(Field(p,"address"));in_addr ip{};Require(inet_pton(AF_INET,v.address.c_str(),&ip)==1);v.metadata_port=Integer(Field(p,"metadata_port"),1,65535);v.metadata_revision=Decimal(Field(p,"metadata_revision"));v.metadata_ready=Boolean(Field(p,"metadata_ready"));const auto& endpoints=Field(p,"endpoints");Array(endpoints,64);std::set<std::string> identities;
  for(const auto& e:endpoints){LocalGatewayEndpoint ep;ep.identity=Runtime(Field(e,"runtime"));ep.identity.registration_id=Decimal(Field(e,"registration_id"));Require(ep.identity.registration_id&&ep.identity.host_id==v.host&&ep.identity.boot_id==v.boot);Require(identities.insert(std::to_string(ep.identity.pid)+":"+std::to_string(ep.identity.start_ticks)+":"+std::to_string(ep.identity.registration_id)).second);ep.application=Runtime(Field(e,"application"),true);ep.host_name=Text(Field(e,"host_name"));ep.application_name=Text(Field(e,"application_name"));ep.element_name=Text(Field(e,"element_name"));ep.name=Text(Field(e,"name"));Require(!ep.name.empty()&&ep.name[0]=='/');ep.kind=Integer(Field(e,"kind"),1,4);ep.role=Integer(Field(e,"role"),1,2);ep.scope=Integer(Field(e,"scope"),0,1);ep.value_type=Type(Field(e,"value_type"));ep.request_type=Type(Field(e,"request_type"));ep.response_type=Type(Field(e,"response_type"));ep.feedback_type=Type(Field(e,"feedback_type"));const auto& groups=Field(e,"groups");Array(groups,64);for(const auto& g:groups)ep.groups.push_back(Text(g));v.endpoints.push_back(std::move(ep));
  }out.push_back(std::move(v));
 }return out;
}
struct Fd{int fd{-1};~Fd(){if(fd>=0)close(fd);}};
void Wait(int fd,short events,Clock::time_point end,const std::atomic<bool>& stop){while(!stop){if(Clock::now()>=end)throw Failure{-ETIMEDOUT};pollfd p{fd,events,0};int n=poll(&p,1,20);if(n>0)return;if(n<0&&errno!=EINTR)throw Failure{-errno};}throw Failure{-ECANCELED};}
void Transfer(int fd,char* bytes,std::size_t size,bool sending,Clock::time_point end,const std::atomic<bool>& stop){std::size_t at=0;while(at<size){if(stop)throw Failure{-ECANCELED};if(Clock::now()>=end)throw Failure{-ETIMEDOUT};auto n=sending?send(fd,bytes+at,size-at,MSG_NOSIGNAL):recv(fd,bytes+at,size-at,0);if(n>0){at+=n;continue;}if(!n)throw Failure{-ECONNRESET};if(errno==EINTR)continue;if(errno!=EAGAIN&&errno!=EWOULDBLOCK)throw Failure{-errno};Wait(fd,sending?POLLOUT:POLLIN,end,stop);}}
std::uint32_t U32(const char* p){std::uint32_t v=0;for(int i=0;i<4;++i)v=(v<<8)|static_cast<unsigned char>(p[i]);return v;}
Json Query(const std::string& path,unsigned op,Clock::time_point end,const std::atomic<bool>& stop){if(path.empty()||path.size()>=sizeof(sockaddr_un::sun_path)||path.find('\0')!=std::string::npos)throw Failure{-EINVAL};Fd s;s.fd=socket(AF_UNIX,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);if(s.fd<0)throw Failure{-errno};sockaddr_un a{};a.sun_family=AF_UNIX;std::copy(path.begin(),path.end(),a.sun_path);if(connect(s.fd,reinterpret_cast<sockaddr*>(&a),sizeof(a))){if(errno!=EINPROGRESS)throw Failure{-errno};Wait(s.fd,POLLOUT,end,stop);int error=0;socklen_t n=sizeof(error);if(getsockopt(s.fd,SOL_SOCKET,SO_ERROR,&error,&n))throw Failure{-errno};if(error)throw Failure{-error};}
 ucred cred{};socklen_t cn=sizeof(cred);if(getsockopt(s.fd,SOL_SOCKET,SO_PEERCRED,&cred,&cn)||cn!=sizeof(cred)||cred.uid!=geteuid())throw Failure{-EACCES};
 char h[16]={0,0,0,0,'K','C','F','L',0,1,0,char(op),0,0,0,0};Transfer(s.fd,h,16,true,end,stop);Transfer(s.fd,h,16,false,end,stop);const auto length=U32(h);Require(length<=MAX_BODY&&U32(h+4)==MAGIC&&h[8]==0&&h[9]==1&&h[10]==0&&static_cast<unsigned char>(h[11])==op);const auto result=U32(h+12);if(result){Require(result>=0x80000000u&&length==0);throw Failure{int(std::int64_t(result)-0x100000000LL)};}Require(length>0);std::string body(length,'\0');Transfer(s.fd,body.data(),body.size(),false,end,stop);
 auto callback=[](int depth,Json::parse_event_t,Json&){Require(depth<=24);return true;};return Json::parse(body,callback);
}
}
struct LocalGatewayClient::Impl {
 std::string path;mutable std::mutex mutex;std::condition_variable cv;std::atomic<bool> stop{false};LocalGatewaySnapshot snapshot;std::thread worker;
 explicit Impl(std::string p):path(std::move(p)){snapshot.socket_path=path;snapshot.connection=LocalGatewayConnection::CONNECTING;worker=std::thread([this]{Run();});}
 void Run(){while(!stop){LocalGatewaySnapshot next;next.socket_path=path;try{auto end=Clock::now()+std::chrono::seconds(1);Status(Query(path,1,end,stop),next);auto peers=Peers(Query(path,2,end,stop));LocalGatewaySnapshot after;Status(Query(path,1,end,stop),after);if(next.host!=after.host||next.boot!=after.boot||next.session!=after.session||next.state!=after.state)throw Failure{-ESTALE};next=std::move(after);next.socket_path=path;next.peers=std::move(peers);next.connection=LocalGatewayConnection::READY;next.observed_at_unix_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}catch(const Failure& e){next={};next.socket_path=path;next.connection=LocalGatewayConnection::ERROR;next.error=e.code;}catch(...){next={};next.socket_path=path;next.connection=LocalGatewayConnection::ERROR;next.error=-EPROTO;}
 std::unique_lock<std::mutex> lock(mutex);if(stop)break;snapshot=std::move(next);cv.wait_for(lock,std::chrono::seconds(1),[this]{return stop.load();});}}
};
LocalGatewayClient::LocalGatewayClient(std::string p):impl_(std::make_unique<Impl>(std::move(p))){}
LocalGatewayClient::~LocalGatewayClient(){Stop();}
void LocalGatewayClient::Stop(){impl_->stop=true;impl_->cv.notify_all();if(impl_->worker.joinable())impl_->worker.join();std::lock_guard<std::mutex> lock(impl_->mutex);impl_->snapshot={};impl_->snapshot.socket_path=impl_->path;}
LocalGatewaySnapshot LocalGatewayClient::Snapshot() const {std::lock_guard<std::mutex> lock(impl_->mutex);return impl_->snapshot;}
}
