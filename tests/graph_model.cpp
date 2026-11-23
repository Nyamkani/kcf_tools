#include "kcf_tool/ui/graph_model.hpp"
#include <algorithm>
#include <cassert>
#include <sstream>
using namespace kcf_tool;
ElementInfo Element(int pid,RuntimeIdentity supervisor={}) {
    ElementInfo e;e.name="same";e.identity={pid,10};e.pid=pid;e.supervisor=supervisor;return e;
}
TopicInfo Topic(int pid,const char* role,int registration=1) {
    TopicInfo t;t.name="/shared";t.identity={{pid,10},static_cast<unsigned>(registration)};t.role=role;
    // Names are intentionally misleading; identity is the only ownership link.
    t.publishers={"not-an-element"};t.subscribers={"same"};return t;
}
std::string Snapshot(const GraphModel& model) {
    std::ostringstream s;
    for(const auto& g:model.groups)s<<g.id<<g.label;
    for(const auto& n:model.nodes){s<<n.id<<n.label<<n.group;
        for(const auto& e:n.endpoints)s<<e.identity.runtime.pid<<e.identity.runtime.process_start_ticks<<e.identity.registration_id<<e.role;}
    for(const auto& e:model.edges)s<<e.from<<e.to<<e.role<<e.endpoint.runtime.pid<<e.endpoint.registration_id;
    return s.str();
}
std::string CommunicationSnapshot(const CommunicationGraphModel& model){
    GraphModel base;base.groups=model.groups;base.nodes=model.nodes;std::ostringstream out;out<<Snapshot(base);
    for(const auto& e:model.edges)out<<e.from<<e.to<<e.topic
        <<e.publisher.runtime.pid<<":"<<e.publisher.runtime.process_start_ticks<<":"<<e.publisher.registration_id
        <<e.subscriber.runtime.pid<<":"<<e.subscriber.runtime.process_start_ticks<<":"<<e.subscriber.registration_id;
    return out.str();
}
void Communication(){
    for(const auto counts:{std::pair<int,int>{1,1},{1,3},{3,1},{3,3}}){
        std::vector<ElementInfo> elements;std::vector<TopicInfo> topics;
        for(int i=0;i<counts.first+counts.second;++i){elements.push_back(Element(200+i));topics.push_back(Topic(200+i,i<counts.first?"PUBLISHER":"SUBSCRIBER"));}
        const auto model=BuildCommunicationGraph({},elements,topics);
        assert(model.nodes.size()==elements.size()&&model.edges.size()==std::size_t(counts.first*counts.second));
        for(const auto& edge:model.edges){
            assert(edge.from=="element:"+std::to_string(edge.publisher.runtime.pid)+":10");
            assert(edge.to=="element:"+std::to_string(edge.subscriber.runtime.pid)+":10");
            assert(edge.topic=="/shared"&&edge.evidence==CommunicationEvidence::Metadata);
        }
    }
    std::vector<ElementInfo> elements{Element(100,{10,1}),Element(101,{10,1}),Element(102)};
    std::vector<TopicInfo> topics{Topic(100,"PUBLISHER"),Topic(101,"SUBSCRIBER")};
    auto pub=Topic(100,"PUBLISHER",2),sub=Topic(101,"SUBSCRIBER",2);pub.name=sub.name="/other";
    topics.push_back(pub);topics.push_back(sub);topics.push_back(topics.front());
    auto model=BuildCommunicationGraph({},elements,topics);assert(model.nodes.size()==3&&model.edges.size()==2);
    // Two registrations on the same runtime are distinct evidence, not duplicates.
    topics.push_back(Topic(100,"PUBLISHER",3));assert(BuildCommunicationGraph({},elements,topics).edges.size()==3);
    const auto before=CommunicationSnapshot(BuildCommunicationGraph({},elements,topics));
    std::reverse(topics.begin(),topics.end());std::reverse(elements.begin(),elements.end());
    assert(CommunicationSnapshot(BuildCommunicationGraph({},elements,topics))==before);
    GraphOptions options;options.group="standalone";auto filtered=BuildCommunicationGraph({},elements,topics,options);
    assert(filtered.nodes.size()==1&&filtered.edges.empty()&&filtered.groups.size()==2);
    options.group="10:1";assert(BuildCommunicationGraph({},elements,topics,options).edges.size()==3);
    options.topics=false;assert(BuildCommunicationGraph({},elements,topics,options).edges.empty());
    options={};options.local=false;assert(BuildCommunicationGraph({},elements,topics,options).nodes.empty());
    options={};options.host="remote-host";assert(BuildCommunicationGraph({},elements,topics,options).nodes.empty());
    options.host="local";options.remote=false;assert(BuildCommunicationGraph({},elements,topics,options).edges.size()==3);
    // Dangling registrations and display names cannot create ownership.
    assert(BuildCommunicationGraph({},elements,{Topic(999,"PUBLISHER"),Topic(101,"SUBSCRIBER")}).edges.empty());
    auto old=Topic(100,"PUBLISHER");old.identity.runtime.process_start_ticks=9;
    assert(BuildCommunicationGraph({},elements,{old,Topic(101,"SUBSCRIBER")}).edges.empty());
    for(auto invalid:{Topic(100,"UNKNOWN"),Topic(100,"PUBLISHER",0)})
        assert(BuildCommunicationGraph({},elements,{invalid,Topic(101,"SUBSCRIBER")}).edges.empty());
    assert(BuildCommunicationGraph({},elements,{Topic(100,"PUBLISHER")}).edges.empty());
    assert(BuildCommunicationGraph({},elements,{Topic(101,"SUBSCRIBER")}).edges.empty());
    pub=Topic(100,"PUBLISHER",40);sub=Topic(101,"SUBSCRIBER",50);
    auto recreated=BuildCommunicationGraph({},elements,{pub,sub});assert(recreated.edges.size()==1);
    pub.identity.registration_id=41;recreated=BuildCommunicationGraph({},elements,{pub,sub});
    assert(recreated.edges.size()==1&&recreated.edges[0].publisher.registration_id==41);
    assert(BuildCommunicationGraph({},elements,{sub}).edges.empty());
    auto conflict=pub;conflict.name="/different";
    assert(BuildCommunicationGraph({},elements,{pub,conflict,sub}).edges.empty());
    std::reverse(elements.begin(),elements.end());
    auto duplicate=elements[0];duplicate.name="conflicting owner";elements.push_back(duplicate);
    assert(BuildCommunicationGraph({},elements,{pub,sub}).edges.empty());elements.pop_back();
    pub.type_id=1;sub.type_id=2;assert(BuildCommunicationGraph({},elements,{pub,sub}).edges.empty());
    pub.type_id=sub.type_id=1;pub.payload_size=4;sub.payload_size=8;
    assert(BuildCommunicationGraph({},elements,{pub,sub}).edges.empty());
    assert(BuildCommunicationGraph({},elements,{}).nodes.size()==3);
    assert(BuildCommunicationGraph({}, {}, {}).nodes.empty());
}
int main() {
    Communication();
    for(const auto counts:{std::pair<int,int>{1,1},{1,3},{3,1},{3,3}}) {
        std::vector<ElementInfo> elements;std::vector<TopicInfo> topics;
        for(int i=0;i<counts.first+counts.second;++i){elements.push_back(Element(200+i));topics.push_back(Topic(200+i,i<counts.first?"PUBLISHER":"SUBSCRIBER"));}
        auto model=BuildGraph({},elements,topics,{},{});
        assert(model.nodes.size()==elements.size()+1 && model.edges.size()==topics.size());
        int publishers=0,subscribers=0;
        for(const auto& edge:model.edges) {
            const auto owner="element:"+std::to_string(edge.endpoint.runtime.pid)+":10";
            if(edge.role=="PUBLISHER"){++publishers;assert(edge.from==owner && edge.to=="1:/shared");}
            else {++subscribers;assert(edge.from=="1:/shared" && edge.to==owner);}
        }
        assert(publishers==counts.first && subscribers==counts.second);
        assert(model.nodes.back().endpoints.size()==topics.size());
    }
    std::vector<ApplicationInfo> apps{{"same","RUNNING",{10,1}},{"same","RUNNING",{10,2}}};
    std::vector<ElementInfo> elements{Element(100,{10,1}),Element(101,{10,2}),Element(102),Element(103)};
    std::vector<TopicInfo> topics{Topic(100,"PUBLISHER"),Topic(101,"SUBSCRIBER"),Topic(102,"SUBSCRIBER")};
    auto all=BuildGraph(apps,elements,topics,{},{});assert(all.groups.size()==3 && all.nodes.size()==5 && all.edges.size()==3);
    GraphOptions filter;filter.group="10:2";auto one=BuildGraph(apps,elements,topics,{},{},filter);
    assert(one.nodes.size()==2 && one.edges.size()==1 && one.edges[0].endpoint.runtime.pid==101);
    filter.group="standalone";assert(BuildGraph(apps,elements,topics,{},{},filter).nodes.size()==3);
    auto stale=Topic(100,"SUBSCRIBER",9);stale.identity.runtime.process_start_ticks=99;topics.push_back(stale);
    all=BuildGraph(apps,elements,topics,{},{});assert(all.edges.size()==3); // no PID-only association
    topics.push_back(Topic(100,"PUBLISHER",2));topics.push_back(topics.back());
    all=BuildGraph(apps,elements,topics,{},{});assert(all.edges.size()==4); // distinct registration retained, exact duplicate ignored
    const auto snapshot=Snapshot(all);
    std::reverse(elements.begin(),elements.end());std::reverse(topics.begin(),topics.end());std::reverse(apps.begin(),apps.end());
    assert(Snapshot(BuildGraph(apps,elements,topics,{},{}))==snapshot);
    std::vector<ParameterInfo> parameters(3);
    for(int i=0;i<3;++i){parameters[i].name="/config";parameters[i].identity={{100+i,10},20};parameters[i].role=i==0?"OWNER":i==1?"CLIENT":"UNKNOWN";}
    std::vector<ServiceInfo> services(2);
    for(int i=0;i<2;++i){services[i].name="/call";services[i].identity={{100+i,10},30};}
    GraphOptions options{false,true,true};auto extra=BuildGraph(apps,elements,topics,parameters,services,options);
    assert(extra.nodes.size()==7 && extra.edges.size()==4);
    for(const auto& edge:extra.edges){
        if(edge.role=="OWNER")assert(edge.from=="element:100:10" && edge.to=="2:/config");
        else if(edge.role=="CLIENT")assert(edge.from=="2:/config" && edge.to=="element:101:10");
        else assert(edge.role=="SERVER" && edge.from.find("element:")==0);
    }
    // Missing owner never creates one; a discovered client has only its own link.
    parameters.erase(parameters.begin());extra=BuildGraph(apps,elements,{},parameters,{},options);
    assert(extra.edges.size()==1 && extra.edges[0].role=="CLIENT");
    assert(BuildGraph(apps,elements,{}, {},{}).nodes.size()==4); // endpoint-free elements survive refresh
    assert(BuildGraph({}, {}, {}, {}, {}).nodes.empty());
}
