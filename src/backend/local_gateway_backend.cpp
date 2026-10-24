#include "kcf_tool/backend/kcf_backend.hpp"
namespace kcf_tool {
int KcfBackend::ConfigureLocalGateway(const std::string& path,bool enabled){
    std::shared_ptr<LocalGatewayClient> old;
    {std::lock_guard<std::mutex> lock(local_gateway_mutex_);old=std::move(local_gateway_);}
    if(old)old->Stop();
    if(!enabled)return 0;
    try{auto client=std::make_shared<LocalGatewayClient>(path);std::lock_guard<std::mutex> lock(local_gateway_mutex_);local_gateway_=std::move(client);return 0;}catch(...){return -EAGAIN;}
}
LocalGatewaySnapshot KcfBackend::GetLocalGatewaySnapshot(){std::lock_guard<std::mutex> lock(local_gateway_mutex_);return local_gateway_?local_gateway_->Snapshot():LocalGatewaySnapshot{};}
}
