#pragma once
#include "kcf_tool/model/network_info.hpp"
#include <array>
#include <chrono>
#include <map>
#include <tuple>

namespace kcf_tool::detail {
// Per Gateway, worker-owned. Call exactly once per completed Diagnostics Query,
// including errors. Discovery refresh copies must never call Observe().
class NetworkRateTracker {
    using Clock=std::chrono::steady_clock;
    using Endpoint=std::tuple<std::string,std::string,std::int32_t,std::uint64_t,std::uint64_t>;
    using Key=std::tuple<std::string,std::string,std::uint64_t,Endpoint,Endpoint,
                         std::uint64_t,std::uint64_t,std::uint64_t,bool>;
    using Counters=std::array<std::uint64_t,9>;
    struct History {Counters baseline,latest;Clock::time_point start,last;};
    std::map<Key,History> history_;
    static Endpoint Identity(const RouteEndpointIdentity& e){return {e.host_id,e.boot_id,e.pid,e.start_ticks,e.registration_id};}
    static Counters Values(const RemoteRoute& r){return {r.packets_sent,r.packets_received,r.samples_published,r.dropped,
        r.duplicate,r.out_of_order,r.missed_sequence,r.malformed,r.local_samples_skipped};}
public:
    void Observe(RemoteNetworkDiagnostics& snapshot,Clock::time_point now) {
        std::map<Key,History> next;
        const bool success=snapshot.status==DiagnosticsState::VALID || snapshot.status==DiagnosticsState::TRUNCATED;
        for(auto& route:snapshot.routes){
            route.rates={};
            // Framework TopicRouteState::ACTIVE == 1. No measurement across
            // WAITING/REJECTED/CLOSED, even when counters remain in history.
            if(!success || route.state!=1)continue;
            const Key key{snapshot.gateway_host,snapshot.gateway_boot,snapshot.gateway_session,
                Identity(route.source),Identity(route.destination),route.source_session,
                route.destination_session,route.route_id,route.sending};
            const auto counters=Values(route);History h{counters,counters,now,now};
            auto old=history_.find(key);bool comparable=old!=history_.end() && now>old->second.last;
            if(comparable)for(std::size_t i=0;i<counters.size();++i)if(counters[i]<old->second.latest[i])comparable=false;
            auto& rates=route.rates;rates.status=NetworkRateState::MEASURING;
            if(comparable){
                h=old->second;h.latest=counters;h.last=now;
                rates.interval_seconds=std::chrono::duration<double>(now-h.start).count();
                if(now-h.start>=std::chrono::milliseconds(500)){
                    Counters delta{};for(std::size_t i=0;i<delta.size();++i)delta[i]=counters[i]-h.baseline[i];
                    rates.status=NetworkRateState::VALID;
                    rates.delta={delta[0],delta[1],delta[2],delta[3],delta[4],delta[5],delta[6],delta[7],delta[8]};
                    rates.send_hz=delta[0]/rates.interval_seconds;rates.receive_hz=delta[1]/rates.interval_seconds;
                    rates.proxy_publish_hz=delta[2]/rates.interval_seconds;rates.local_skipped_per_second=delta[8]/rates.interval_seconds;
                    h={counters,counters,now,now};
                }
            }
            next.emplace(key,std::move(h));
        }
        // Missing routes, including truncated omissions, break continuity. We
        // do not claim they were removed; we simply cannot measure across the gap.
        history_=std::move(next);
    }
};
}
