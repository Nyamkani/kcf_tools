#include "kcf_tool/model/topic_plot_history.hpp"
#include "kcf_tool/model/remote_control.hpp"
#include <cassert>
#include <cerrno>
#include <chrono>
#include <sstream>
#include <iostream>
using namespace std::chrono_literals;
using kcf_tool::TopicPlotHistory;
using kcf_tool::FieldValue;
using kcf_tool::DataSnapshot;
using kcf_tool::ValueKind;
int main() {
    TopicPlotHistory history;
    auto now=TopicPlotHistory::Clock::now();
    DataSnapshot d;
    d.type_id=9;
    d.sequence=100;
    FieldValue numeric;numeric.name="wheel";numeric.kind=ValueKind::FLOATING_POINT;numeric.type_name="float";
    numeric.value="1.5";
    FieldValue array;array.name="imu";array.kind=ValueKind::SIGNED_INTEGER;array.array_values={"-2","3"};
    FieldValue text;text.name="debug";text.kind=ValueKind::STRING;text.value="skip";
    d.fields={numeric,array,text};
    history.Reset("topic/one",d.type_id);
    assert(history.Observe(d,now)==TopicPlotHistory::ObserveResult::RECORDED);
    assert(history.Fields().size()==3 && history.Fields()[0]=="wheel" &&
           history.Fields()[1]=="imu[0]" && history.Fields()[2]=="imu[1]");
    assert(history.Records().front().values[0]==1.5);
    assert(history.Observe(d,now+10ms)==TopicPlotHistory::ObserveResult::UNCHANGED);
    d.sequence=106;d.fields[0].value="2.75";
    assert(history.Observe(d,now+100ms)==TopicPlotHistory::ObserveResult::RECORDED);
    assert(history.Records().back().unobserved_sequence_steps==5);
    assert(history.Observations()==2 && history.UnobservedSequenceSteps()==5);
    std::ostringstream csv;
    assert(history.WriteCsv(csv));
    assert(csv.str().find("unobserved_sequence_steps")!=std::string::npos);
    assert(csv.str().find("observed_elapsed_ms,transport_sequence,unobserved_sequence_steps,")!=std::string::npos);
    assert(csv.str().find("2.75")!=std::string::npos);
    assert(csv.str().find("100,0")!=std::string::npos);
    d.sequence=3;
    assert(history.Observe(d,now+200ms)==TopicPlotHistory::ObserveResult::NEW_SEGMENT);
    assert(history.Records().size()==1 && history.UnobservedSequenceSteps()==0);
    d.type_id=10;
    assert(history.Observe(d,now+300ms)==TopicPlotHistory::ObserveResult::NEW_SEGMENT);
    d.fields[0].value="NaN";
    assert(history.Observe(d,now+400ms)==TopicPlotHistory::ObserveResult::INVALID_SAMPLE);
    assert(history.Records().size()==1);
    d.fields[0].value="4";
    history.Reset("source2",10);
    for(std::size_t i=0;i<TopicPlotHistory::MAX_RECORDS+24;++i) {
        d.sequence=i+1;
        assert(history.Observe(d,now+std::chrono::milliseconds(i+1))==TopicPlotHistory::ObserveResult::RECORDED);
    }
    assert(history.Records().size()==TopicPlotHistory::MAX_RECORDS);
    assert(history.Observations()==TopicPlotHistory::MAX_RECORDS+24);
    assert(history.DroppedByCapacity()==24);
    assert(history.Records().front().sequence==25);
    assert(history.Observe(d,now-1ms)==TopicPlotHistory::ObserveResult::INVALID_SAMPLE);
    // CSV quoting and bounded observation/field contracts.
    TopicPlotHistory escaped;
    DataSnapshot special{};special.type_id=12;special.sequence=1;
    FieldValue quoted;quoted.name="wheel,\"left\"";quoted.kind=ValueKind::FLOATING_POINT;
    quoted.value="1.25";special.fields={quoted};
    escaped.Reset("robot,\"test\"",12);
    assert(escaped.Observe(special,now+1s)==TopicPlotHistory::ObserveResult::RECORDED);
    std::ostringstream csv_quoted;assert(escaped.WriteCsv(csv_quoted));
    assert(csv_quoted.str().find("\"wheel,\"\"left\"\"\"")!=std::string::npos);
    assert(csv_quoted.str().find("\"robot,\"\"test\"\"\"")!=std::string::npos);
    special.sequence=2;special.fields[0].name="wheel_right";
    assert(escaped.Observe(special,now+1100ms)==TopicPlotHistory::ObserveResult::NEW_SEGMENT);
    assert(escaped.Records().size()==1 && escaped.UnobservedSequenceSteps()==0);
    special.fields[0].kind=ValueKind::STRING;
    assert(escaped.Observe(special,now+1200ms)==TopicPlotHistory::ObserveResult::NO_NUMERIC_FIELDS);
    assert(escaped.Records().size()==1);
    special.fields[0].kind=ValueKind::FLOATING_POINT;special.fields[0].value="inf";
    assert(escaped.Observe(special,now+1300ms)==TopicPlotHistory::ObserveResult::INVALID_SAMPLE);
    DataSnapshot large{};large.type_id=13;large.sequence=1;
    FieldValue excess;excess.name="array";excess.kind=ValueKind::SIGNED_INTEGER;
    excess.array_values.assign(TopicPlotHistory::MAX_NUMERIC_FIELDS+1,"1");
    large.fields={excess};TopicPlotHistory capped;capped.Reset("large",large.type_id);
    assert(capped.Observe(large,now+1400ms)==TopicPlotHistory::ObserveResult::INVALID_SAMPLE);
    assert(capped.Records().empty());
    // Sender diagnostics are matched only through complete route identity;
    // same Topic name or route ID must never authorize a false correlation.
    kcf_tool::NetworkSnapshot network;
    kcf_tool::RemoteActivity activity;activity.topic=true;activity.handle=9;
    activity.endpoint.name="/shared";activity.route_id=100;
    activity.route_source={"source","boot-src",123,456,10};
    activity.route_destination={"receiver","boot-dst",321,654,20};
    activity.source_session=55;activity.destination_session=77;
    kcf_tool::RemoteHost sender;sender.id="source";sender.boot_id="boot-src";
    sender.session=55;sender.state="ONLINE";
    sender.diagnostics.status=kcf_tool::DiagnosticsState::VALID;
    sender.diagnostics.gateway_host=sender.id;sender.diagnostics.gateway_boot=sender.boot_id;
    sender.diagnostics.gateway_session=sender.session;sender.diagnostics.observed_at_unix_ms=10000;
    kcf_tool::RemoteRoute route;route.sending=true;route.route_id=activity.route_id;
    route.name=activity.endpoint.name;route.source=activity.route_source;
    route.destination=activity.route_destination;
    route.source_session=activity.source_session;route.destination_session=activity.destination_session;
    sender.diagnostics.routes.push_back(route);network.hosts.push_back(sender);
    const auto match=[&]{return kcf_tool::FindRemoteTopicSenderRoute(network,activity,10500);};
    assert(match() && match()->route_id==100);
    activity.destination_session++;assert(!match());activity.destination_session--;
    activity.route_destination.registration_id++;assert(!match());activity.route_destination.registration_id--;
    activity.route_source.start_ticks++;assert(!match());activity.route_source.start_ticks--;
    activity.route_id++;assert(!match());activity.route_id--;
    activity.endpoint.name="/other";assert(!match());activity.endpoint.name="/shared";
    activity.communication_lost=true;assert(!match());activity.communication_lost=false;
    network.hosts[0].diagnostics.routes[0].sending=false;assert(!match());
    network.hosts[0].diagnostics.routes[0].sending=true;
    network.hosts[0].diagnostics.gateway_session++;assert(!match());
    network.hosts[0].diagnostics.gateway_session--;
    network.hosts[0].diagnostics.status=kcf_tool::DiagnosticsState::LOST;assert(!match());
    network.hosts[0].diagnostics.status=kcf_tool::DiagnosticsState::TRUNCATED;assert(match());
    network.hosts[0].diagnostics.observed_at_unix_ms=1;assert(!match());
    network.hosts[0].diagnostics.observed_at_unix_ms=20000;assert(!match());
    network.hosts[0].diagnostics.observed_at_unix_ms=10000;assert(match());
    network.hosts[0].diagnostics.routes.push_back(route);assert(!match()); // ambiguous
    network.hosts[0].diagnostics.routes.pop_back();
    network.hosts[0].state="LOST";assert(!match());network.hosts[0].state="ONLINE";
    std::cout<<"Remote Topic sender diagnostic strict identity/freshness contracts PASS\\n";
    std::cout<<"Bounded local Topic plot numeric/gap/segment/CSV contracts PASS\n";
}
