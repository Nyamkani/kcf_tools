#include "kcf_tool/ui/graph_model.hpp"
#include <algorithm>
#include <map>
#include <tuple>

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
}
