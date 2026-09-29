#include "kcf_tool/ui/graph_model.hpp"
#include <map>
#include <set>
namespace kcf_tool {
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
    return model;
}
}
