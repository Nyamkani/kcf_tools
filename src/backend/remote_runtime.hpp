#pragma once
// Owned within NetworkObserver's Discovery lifetime. All calls run on one control
// worker; Framework Action/Topic workers continue independently of these calls.
#include "value_codec.hpp"
#include "kcf_tool/model/remote_control.hpp"
#include "kcf/network/remote_topic_client.hpp"
#include <map>
namespace kcf_tool {
class RemoteRuntime {
public:
    RemoteRuntime(kcf::network::PeerDiscovery&,const kcf::network::DiscoveryConfig&);
    ~RemoteRuntime();
    void CancelIO();
    int StartError() const { return error_; }
    void StopTopics(){topics_.Stop();}
    RemoteReply Execute(const RemoteCommand&);
private:
    struct Action {RemoteEndpoint endpoint; kcf::TypeDescriptor feedback,result;std::shared_ptr<kcf::network::RemoteControlActionHandle> handle;};
    struct Topic {RemoteEndpoint endpoint;std::shared_ptr<kcf::network::RemoteTopicSubscription> handle;};
    kcf::network::PeerDiscovery& discovery_;
    kcf::network::RemoteControlClient control_;
    kcf::network::RemoteTopicClient topics_;
    std::map<std::uint64_t,Action> actions_;std::map<std::uint64_t,Topic> subscriptions_;
    int error_{0};
    bool Current(const RemoteEndpoint&) const;
    int Descriptor(const RemoteEndpoint&,kcf::network::DescriptorRole,kcf::network::TypeIdentity,kcf::TypeDescriptor&,std::uint32_t);
};
}

#include <condition_variable>
#include <deque>
#include <thread>
namespace kcf_tool {
class RemoteExecutor {
public:
    RemoteExecutor(kcf::network::PeerDiscovery& d,const kcf::network::DiscoveryConfig& c):runtime_(d,c),worker_([this]{Run();}){}
    ~RemoteExecutor(){Stop();}
    int StartError() const { return runtime_.StartError(); }
    std::future<RemoteReply> Submit(const RemoteCommand& c){std::lock_guard<std::mutex> lock(mutex_);if(stopping_)return RemoteUnavailable(-ECANCELED);if(queue_.size()>=32)return RemoteUnavailable(-EAGAIN);
        Job j;j.command=c;auto f=j.promise.get_future();queue_.push_back(std::move(j));wake_.notify_one();return f;}
    void Stop(){std::lock_guard<std::mutex> stop(stop_mutex_);{std::lock_guard<std::mutex> lock(mutex_);stopping_=true;wake_.notify_all();}runtime_.CancelIO();if(worker_.joinable())worker_.join();runtime_.StopTopics();}
private:
    struct Job {RemoteCommand command;std::promise<RemoteReply> promise;};
    void Run(){for(;;){Job job;{std::unique_lock<std::mutex> lock(mutex_);wake_.wait(lock,[&]{return stopping_||!queue_.empty();});if(stopping_){for(auto& j:queue_){RemoteReply r;r.error=-ECANCELED;j.promise.set_value(r);}queue_.clear();return;}job=std::move(queue_.front());queue_.pop_front();}
        try{job.promise.set_value(runtime_.Execute(job.command));}catch(...){RemoteReply r;r.error=-EIO;job.promise.set_value(r);}}}
    RemoteRuntime runtime_;std::mutex mutex_,stop_mutex_;std::condition_variable wake_;bool stopping_{false};std::deque<Job> queue_;std::thread worker_;
};
}
