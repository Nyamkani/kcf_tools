#include "kcf_tool/model/topic_plot_history.hpp"
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
    std::cout<<"Bounded local Topic plot numeric/gap/segment/CSV contracts PASS\n";
}
