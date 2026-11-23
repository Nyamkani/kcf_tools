#include "kcf_tool/ui/graph_model.hpp"
#include <algorithm>
#include <map>
#include <tuple>
#include <set>

namespace kcf_tool {
namespace {
std::string Identity(const RuntimeIdentity& id) {
    return std::to_string(id.pid)+":"+std::to_string(id.process_start_ticks);
}
auto EndpointKey(const EndpointIdentity& id) {
    return std::make_tuple(id.runtime.pid,id.runtime.process_start_ticks,id.registration_id);
}
}
GraphModel BuildGraph(const std::vector<ApplicationInfo>& applications,
                      const std::vector<ElementInfo>& elements,
                      const std::vector<TopicInfo>& topics,
                      const std::vector<ParameterInfo>& parameters,
                      const std::vector<ServiceInfo>& services,const GraphOptions& options) {
    GraphModel model;
    std::map<std::string,std::string> groups;
    for(const auto& app:applications) {
        const auto id=Identity(app.supervisor);
        groups[id]=app.name+" ["+id+"]";
    }
    std::map<std::string,GraphNode> nodes;
    for(const auto& element:elements) {
        const auto group=element.supervisor.pid>0?Identity(element.supervisor):"standalone";
        if(!groups.count(group))groups[group]=group=="standalone"?"Standalone":
            (element.application_name.empty()?"Supervisor":element.application_name)+" ["+group+"]";
        if(!options.group.empty() && options.group!=group)continue;
        const auto id="element:"+Identity(element.identity);
        nodes[id]={id,element.name,group,GraphKind::Element,element.identity,{}};
    }
    // Keep all filter choices even when a group has no visible endpoints.
    for(const auto& group:groups)model.groups.push_back({group.first,group.second});
    const auto add=[&](GraphKind kind,const std::string& name,const EndpointIdentity& identity,
                       const std::string& role,bool outgoing) {
        const auto element="element:"+Identity(identity.runtime);
        const auto owner=nodes.find(element);
        if(!options.group.empty() && owner==nodes.end())return;
        // Topic/Parameter names identify logical channels, never Element owners.
        // Service servers remain individual registrations; no client inference.
        const auto id=std::to_string(static_cast<int>(kind))+":"+name+
            (kind==GraphKind::Service?":"+Identity(identity.runtime)+":"+std::to_string(identity.registration_id):"");
        auto inserted=nodes.emplace(id,GraphNode{id,name,{},kind,{},{}});
        auto& endpoint_list=inserted.first->second.endpoints;
        if(std::any_of(endpoint_list.begin(),endpoint_list.end(),[&](const auto& e){return e.identity==identity && e.role==role;}))return;
        endpoint_list.push_back({identity,role});
        // An endpoint may outlive its Runtime in a non-atomic discovery snapshot.
        if(owner!=nodes.end())model.edges.push_back({outgoing?element:id,outgoing?id:element,role,identity});
    };
    if(options.topics)for(const auto& topic:topics)
        if(topic.role=="PUBLISHER"||topic.role=="SUBSCRIBER")
            add(GraphKind::Topic,topic.name,topic.identity,topic.role,topic.role=="PUBLISHER");
    if(options.parameters)for(const auto& parameter:parameters)
        if(parameter.role=="OWNER"||parameter.role=="CLIENT")
            add(GraphKind::Parameter,parameter.name,parameter.identity,parameter.role,parameter.role=="OWNER");
    if(options.services)for(const auto& service:services)
        add(GraphKind::Service,service.name,service.identity,"SERVER",true);
    for(auto& entry:nodes) {
        auto& node=entry.second;
        std::sort(node.endpoints.begin(),node.endpoints.end(),[](const auto& a,const auto& b){
            return std::make_tuple(EndpointKey(a.identity),a.role)<std::make_tuple(EndpointKey(b.identity),b.role);});
        model.nodes.push_back(std::move(node));
    }
    std::sort(model.nodes.begin(),model.nodes.end(),[](const auto& a,const auto& b){
        return std::tie(a.kind,a.group,a.label,a.id)<std::tie(b.kind,b.group,b.label,b.id);});
    std::sort(model.edges.begin(),model.edges.end(),[](const auto& a,const auto& b){
        return std::make_tuple(a.from,a.to,a.role,EndpointKey(a.endpoint))<std::make_tuple(b.from,b.to,b.role,EndpointKey(b.endpoint));});
    return model;
}
CommunicationGraphModel BuildCommunicationGraph(
    const std::vector<ApplicationInfo>& applications,
    const std::vector<ElementInfo>& elements,
    const std::vector<TopicInfo>& topics,const GraphOptions& options) {
    CommunicationGraphModel model;
    if(!options.local || (!options.host.empty() && options.host!="local"))return model;
    // Conflicting records for one runtime are not sufficient ownership evidence.
    std::map<std::string,ElementInfo> runtimes;
    std::set<std::string> ambiguous_runtimes;
    for(const auto& e:elements){
        if(e.identity.pid<=0 || !e.identity.process_start_ticks)continue;
        const auto key=Identity(e.identity);
        auto [it,inserted]=runtimes.emplace(key,e);
        if(!inserted && std::make_tuple(e.name,Identity(e.supervisor),e.application_name)!=
            std::make_tuple(it->second.name,Identity(it->second.supervisor),it->second.application_name))ambiguous_runtimes.insert(key);
    }
    std::vector<ElementInfo> owners;
    for(const auto& [key,e]:runtimes)if(!ambiguous_runtimes.count(key))owners.push_back(e);
    auto ownership=BuildGraph(applications,owners,{}, {},{},options);
    model.groups=std::move(ownership.groups);model.nodes=std::move(ownership.nodes);
    if(!options.topics)return model;
    std::set<std::string> visible;
    for(const auto& node:model.nodes)visible.insert(node.id);
    using Registration=decltype(EndpointKey(EndpointIdentity{}));
    std::map<Registration,TopicInfo> registrations;
    std::set<Registration> ambiguous;
    for(const auto& topic:topics){
        const auto key=EndpointKey(topic.identity);
        // Do not infer Runtime ownership from publisher/subscriber name lists.
        if(topic.identity.runtime.pid<=0 || !topic.identity.runtime.process_start_ticks || !topic.identity.registration_id)continue;
        auto [it,inserted]=registrations.emplace(key,topic);
        if(!inserted && std::tie(topic.name,topic.role,topic.type_id,topic.payload_size)!=
            std::tie(it->second.name,it->second.role,it->second.type_id,it->second.payload_size))ambiguous.insert(key);
    }
    struct Channel {std::vector<const TopicInfo*> publishers,subscribers;};
    std::map<std::string,Channel> channels;
    for(const auto& [key,t]:registrations){
        if(ambiguous.count(key)||t.name.empty()||t.name.front()!='/'||
            !visible.count("element:"+Identity(t.identity.runtime)))continue;
        if(t.role=="PUBLISHER")channels[t.name].publishers.push_back(&t);
        else if(t.role=="SUBSCRIBER")channels[t.name].subscribers.push_back(&t);
    }
    for(const auto& [name,channel]:channels)for(const auto* pub:channel.publishers)for(const auto* sub:channel.subscribers){
        // Known contradictory type metadata cannot establish a relationship.
        // Unknown type information is allowed only as registration evidence.
        if((pub->type_id&&sub->type_id&&pub->type_id!=sub->type_id)||
           (pub->payload_size&&sub->payload_size&&pub->payload_size!=sub->payload_size))continue;
        model.edges.push_back({"element:"+Identity(pub->identity.runtime),
            "element:"+Identity(sub->identity.runtime),name,pub->identity,sub->identity});
    }
    std::sort(model.edges.begin(),model.edges.end(),[](const auto& a,const auto& b){
        return std::make_tuple(a.from,a.to,a.topic,EndpointKey(a.publisher),EndpointKey(a.subscriber))<
               std::make_tuple(b.from,b.to,b.topic,EndpointKey(b.publisher),EndpointKey(b.subscriber));
    });
    return model;
}

}
