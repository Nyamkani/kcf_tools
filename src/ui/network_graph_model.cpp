#include "kcf_tool/ui/graph_model.hpp"
#include <map>
#include <set>
#include <sstream>
#include <iomanip>
#include <locale>
#include <chrono>
#include <tuple>
namespace kcf_tool {
namespace {
std::string EndpointKey(const RouteEndpointIdentity& e){
    return e.host_id+":"+e.boot_id+":"+std::to_string(e.pid)+":"+std::to_string(e.start_ticks)+":"+std::to_string(e.registration_id);
}
bool MatchesEndpoint(const RemoteHost& host,const RemoteEndpoint& endpoint,const RemoteRoute& r){
    const bool publisher=endpoint.role=="PUBLISHER" && r.sending;
    const bool subscriber=endpoint.role=="SUBSCRIBER" && !r.sending;
    if(endpoint.kind!="T" || (!publisher && !subscriber))return false;
    const auto& local=publisher?r.source:r.destination;
    return EndpointKey(local)==endpoint.id && local.host_id==host.id && local.boot_id==host.boot_id &&
        (publisher?r.source_session:r.destination_session)==host.session;
}
// At most ten queries/s across up to 64 peers: allow a full round plus delay.
// Re-evaluated on manual Refresh/filter redraw; never infer liveness between draws.
bool FreshDiagnostics(const RemoteHost& h,std::uint64_t now){
    const auto& d=h.diagnostics;
    return h.state=="ONLINE" && h.compatibility=="COMPATIBLE" && h.metadata_ready &&
        (d.status==DiagnosticsState::VALID || d.status==DiagnosticsState::TRUNCATED) &&
        d.gateway_host==h.id && d.gateway_boot==h.boot_id && d.gateway_session==h.session &&
        d.observed_at_unix_ms!=0 && d.observed_at_unix_ms<=now && now-d.observed_at_unix_ms<=10000;
}
void AddVerifiedRoutes(GraphModel& model,const NetworkSnapshot& network){
    const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::set<std::string> visible;
    for(const auto& n:model.nodes)if(n.kind==GraphKind::Topic && !n.host.empty())visible.insert(n.id);
    using Key=std::tuple<std::string,std::string,std::uint64_t,std::uint64_t,std::uint64_t>;
    struct Evidence { const RemoteRoute* send{nullptr};const RemoteRoute* receive{nullptr}; };
    std::map<Key,Evidence> evidence;
    for(const auto& h:network.hosts){
        if(!FreshDiagnostics(h,now))continue;
        for(const auto& r:h.diagnostics.routes){
            if(r.state!=1)continue;
            bool registered=false;
            for(const auto& e:h.endpoints)if(MatchesEndpoint(h,e,r) && visible.count("remote-endpoint:"+e.id)){registered=true;break;}
            if(!registered)continue;
            auto& pair=evidence[{EndpointKey(r.source),EndpointKey(r.destination),r.source_session,r.destination_session,r.route_id}];
            (r.sending?pair.send:pair.receive)=&r;
        }
    }
    for(const auto& entry:evidence){
        const auto& pair=entry.second;if(!pair.send || !pair.receive)continue;
        const auto& r=*pair.send;
        std::ostringstream out;out.imbue(std::locale::classic());out<<std::fixed<<std::setprecision(2);
        out<<"Verified ACTIVE network route / route_id="<<r.route_id;
        const auto rate=[&](const char* label,const NetworkRouteRates& rates,double value){
            out<<"\n"<<label<<": ";
            if(rates.status==NetworkRateState::MEASURING)out<<"Measuring";
            else if(rates.status==NetworkRateState::VALID)out<<value;
            else out<<"Unavailable";
        };
        rate("Network Send Hz",r.rates,r.rates.send_hz);
        rate("Network Receive Hz",pair.receive->rates,pair.receive->rates.receive_hz);
        out<<"\nBoth observations within 10 s at graph refresh; measurement intervals are independent.";
        model.edges.push_back({"remote-endpoint:"+EndpointKey(r.source),"remote-endpoint:"+EndpointKey(r.destination),
            "Verified network route",{},true,out.str()});
    }
}
const char* RouteState(std::uint8_t state){
    switch(state){case 0:return "WAITING_SUBSCRIBER";case 1:return "ACTIVE";case 2:return "REJECTED";case 3:return "CLOSED";default:return "UNKNOWN";}
}
std::string TopicDiagnostics(const RemoteHost& host,const RemoteEndpoint& endpoint){
    const auto& d=host.diagnostics;std::ostringstream out;out.imbue(std::locale::classic());out<<std::fixed<<std::setprecision(2);
    out<<"\n\nNetwork Topic Diagnostics (manual Refresh)\n";
    if(host.state!="ONLINE"){out<<"Unavailable: Gateway "<<host.state;return out.str();}
    if(host.compatibility!="COMPATIBLE"){out<<"Unavailable: incompatible Gateway";return out.str();}
    switch(d.status){
    case DiagnosticsState::UNAVAILABLE:out<<"Unavailable: Diagnostics Client API unavailable in this build";return out.str();
    case DiagnosticsState::UNSUPPORTED:out<<"Unavailable: NETWORK_DIAGNOSTICS capability unsupported / disabled";return out.str();
    case DiagnosticsState::INCOMPATIBLE:out<<"Unavailable: incompatible Gateway";return out.str();
    case DiagnosticsState::PENDING:out<<"Pending: awaiting first Diagnostics observation";return out.str();
    case DiagnosticsState::LOST:out<<"Unavailable: Gateway LOST";return out.str();
    case DiagnosticsState::ERROR:out<<"Unavailable: Query error "<<d.error;return out.str();
    case DiagnosticsState::TIMEOUT:out<<"Unavailable: Query timeout "<<d.error;return out.str();
    case DiagnosticsState::VALID:case DiagnosticsState::TRUNCATED:break;
    }
    if(d.gateway_host!=host.id || d.gateway_boot!=host.boot_id || d.gateway_session!=host.session){
        out<<"Unavailable: stale Gateway identity/session";return out.str();
    }
    const bool partial=d.truncated || d.status==DiagnosticsState::TRUNCATED;
    out<<"Gateway: "<<d.gateway_host<<" / "<<d.gateway_boot<<" / session "<<d.gateway_session
       <<"\nObservation time (Unix ms): "<<d.observed_at_unix_ms<<"\nRoutes included/total: "<<d.routes.size()<<"/"<<d.total_routes<<"\n";
    if(partial)out<<"Truncated snapshot: omitted routes are unknown, not absent.\n";
    std::size_t count=0;
    for(const auto& r:d.routes){
        const bool publisher=endpoint.role=="PUBLISHER" && r.sending;
        if(!MatchesEndpoint(host,endpoint,r))continue;
        out<<"\nRoute "<<++count<<" (route_id="<<r.route_id<<")\nTopic: "<<r.name
           <<"\nState: "<<RouteState(r.state)<<" ("<<unsigned(r.state)<<")\nSending: "<<(r.sending?"true":"false")
           <<"\nSource: "<<EndpointKey(r.source)<<" / session "<<r.source_session
           <<"\nDestination: "<<EndpointKey(r.destination)<<" / session "<<r.destination_session
           <<"\nError: "<<r.error<<" / reason: "<<unsigned(r.reason)<<"\n";
        const auto rate=[&](const char* label,double value){
            out<<label<<": ";
            if(r.state!=1 || r.rates.status==NetworkRateState::INVALID)out<<"Unavailable";
            else if(r.rates.status==NetworkRateState::MEASURING)out<<"Measuring";
            else out<<value;
            out<<"\n";
        };
        if(publisher){rate("Network Send Hz",r.rates.send_hz);rate("Local Skipped/s",r.rates.local_skipped_per_second);}
        else {rate("Network Receive Hz",r.rates.receive_hz);rate("Proxy Publish Hz",r.rates.proxy_publish_hz);
            out<<"Proxy Publish is not application subscriber processing rate.\n";}
        if(r.state==1 && r.rates.status==NetworkRateState::VALID)out<<"Measured interval (s): "<<r.rates.interval_seconds<<"\n";
        out<<"Counters: packets_sent="<<r.packets_sent<<", packets_received="<<r.packets_received
           <<", samples_published="<<r.samples_published<<"\nlocal_samples_skipped="<<r.local_samples_skipped
           <<", dropped="<<r.dropped<<", duplicate="<<r.duplicate<<", out_of_order="<<r.out_of_order
           <<", missed_sequence="<<r.missed_sequence<<", malformed="<<r.malformed<<"\n";
    }
    if(!count)out<<(partial?"No matching route in partial snapshot; absence is unknown.":"No matching diagnostics route for this endpoint.");
    return out.str();
}
}

GraphModel AddRemoteGraph(GraphModel model,const NetworkSnapshot& network,const GraphOptions& options) {
    if(!options.local || (!options.host.empty() && options.host!="local"))model={};
    if(!options.remote)return model;
    for(const auto& host:network.hosts) {
        if(!options.host.empty() && options.host!=host.id)continue;
        const bool lost=host.state=="LOST";
        const auto description="REMOTE Host "+host.label+"\nHost: "+host.id+"\nBoot: "+host.boot_id+
            "\nAddress: "+host.address+"\n"+host.state+"\nCompatibility: "+host.compatibility+
            "\nFramework: "+host.framework_version+"\nNetwork: "+host.network_version+
            "\nCapabilities: "+host.capabilities+"\nSession: "+std::to_string(host.session)+
            "\nMetadata revision: "+std::to_string(host.revision)+"\nMetadata ready: "+(host.metadata_ready?"yes":"no")+
            "\nLast announce age (snapshot ms): "+std::to_string(host.age_ms)+"\n"+network.route_status;
        GraphNode heading;heading.id="remote-host:"+host.id;heading.label=host.label+" "+host.state+
            " "+(host.compatibility=="COMPATIBLE"?host.compatibility:"INCOMPATIBLE: "+host.compatibility)+
            " FW "+host.framework_version+" Net "+host.network_version;
        heading.kind=GraphKind::Host;heading.host=host.id;heading.detail=description;heading.lost=lost;
        std::map<std::string,GraphNode> nodes;std::map<std::string,std::string> groups;
        for(const auto& e:host.endpoints) {
            const auto group="remote-app:"+e.application_id;
            groups[group]=e.application+" ["+e.application_id+"]";
            if(!options.group.empty() && options.group!=group)continue;
            const auto runtime="remote-runtime:"+e.runtime_id;
            GraphNode element;element.id=runtime;element.label=e.element;element.group=group;
            element.kind=GraphKind::Element;element.host=host.id;element.lost=lost;
            element.detail=description+"\nApplication: "+e.application+"\nApplication identity: "+e.application_id+
                "\nElement: "+e.element+"\nRuntime: "+e.runtime_id+"\nRuntime liveness is not announced separately";
            nodes.emplace(runtime,std::move(element));
            if((e.kind=="T" && !options.topics)||(e.kind=="P" && !options.parameters)||
               (e.kind=="S" && !options.services)||(e.kind=="A" && !options.actions))continue;
            GraphNode endpoint;endpoint.id="remote-endpoint:"+e.id;endpoint.label=e.kind+": "+e.name+" ["+e.role+"]"+(e.diagnostic.empty()?"":" [PAIR WARNING]");
            endpoint.kind=e.kind=="T"?GraphKind::Topic:e.kind=="P"?GraphKind::Parameter:e.kind=="S"?GraphKind::Service:GraphKind::Action;
            endpoint.group=group;endpoint.host=host.id;endpoint.lost=lost;
            endpoint.detail=description+"\nApplication: "+e.application+"\nElement: "+e.element+
                "\nEndpoint identity: "+e.id+"\n"+endpoint.label+"\nScope: "+e.scope+"\n"+e.type+
                "\nPair compatibility diagnostics (not routes):\n"+(e.diagnostic.empty()?"Not assessed / no incompatible pair observed":e.diagnostic)+
                "\nProxy status: not exposed by Discovery; publisher role alone does not identify a proxy";
            if(e.kind=="T")endpoint.detail+=TopicDiagnostics(host,e);
            nodes.emplace(endpoint.id,endpoint);
            // Same-host ownership only. Each registration remains distinct;
            // never infer an inter-host route or Service/Action client.
            if(e.role=="PUBLISHER" || e.role=="OWNER" || e.role=="SERVER")
                model.edges.push_back({runtime,endpoint.id,e.role+" metadata ownership",{}});
            else if(e.kind=="T" || e.kind=="P")
                model.edges.push_back({endpoint.id,runtime,e.role+" metadata ownership",{}});
        }
        if(options.group.empty() || groups.count(options.group))model.nodes.push_back(std::move(heading));
        for(const auto& group:groups)model.groups.push_back({group.first,group.second,host.id});
        for(auto& entry:nodes)model.nodes.push_back(std::move(entry.second));
    }
    AddVerifiedRoutes(model,network);
    return model;
}
}
