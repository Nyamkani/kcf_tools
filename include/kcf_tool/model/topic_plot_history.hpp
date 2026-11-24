#pragma once
#include "kcf_tool/model/dynamic_value.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <deque>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace kcf_tool {
// Records snapshots OBSERVED by the Tools polling loop (not every publish).
// Source timestamps are deliberately not used: only observer-local steady_clock
// exists for this sampling path. No end-to-end latency is inferred.
class TopicPlotHistory {
public:
    using Clock=std::chrono::steady_clock;
    static constexpr std::size_t MAX_RECORDS=600;
    static constexpr std::size_t MAX_NUMERIC_FIELDS=64;
    struct Record {
        Clock::time_point observed_at{};
        std::uint64_t sequence{0};
        std::uint64_t unobserved_sequence_steps{0};
        std::vector<double> values;
    };
    enum class ObserveResult { RECORDED, UNCHANGED, NO_NUMERIC_FIELDS, NEW_SEGMENT, INVALID_SAMPLE };
    void Reset(std::string source={},std::uint64_t type_id=0) {
        source_=std::move(source);type_id_=type_id;
        records_.clear();fields_.clear();observations_=0;gaps_=0;dropped_by_capacity_=0;
    }
    ObserveResult Observe(const DataSnapshot& snapshot,Clock::time_point when) {
        if(when==Clock::time_point{})return ObserveResult::INVALID_SAMPLE;
        std::vector<std::string> names;std::vector<double> values;
        if(!Flatten(snapshot,names,values))return ObserveResult::INVALID_SAMPLE;
        if(names.empty())return ObserveResult::NO_NUMERIC_FIELDS;
        if(!records_.empty() && when<records_.back().observed_at)return ObserveResult::INVALID_SAMPLE;
        if(!records_.empty() && type_id_==snapshot.type_id && fields_==names &&
            snapshot.sequence==records_.back().sequence)return ObserveResult::UNCHANGED;
        bool split=false;
        if(!records_.empty() && (fields_!=names || (type_id_!=0 && snapshot.type_id!=0 && type_id_!=snapshot.type_id) ||
            snapshot.sequence<records_.back().sequence)) {
            // Do not connect points from different types / sequence generations.
            Reset(source_,snapshot.type_id);
            split=true;
        }
        if(fields_.empty()){fields_=std::move(names);type_id_=snapshot.type_id;}
        std::uint64_t gap=0;
        if(!records_.empty() && snapshot.sequence>records_.back().sequence)gap=snapshot.sequence-records_.back().sequence-1;
        if(std::numeric_limits<std::uint64_t>::max()-gaps_<gap)gaps_=std::numeric_limits<std::uint64_t>::max();
        else gaps_+=gap;
        records_.push_back({when,snapshot.sequence,gap,std::move(values)});
        ++observations_;
        if(records_.size()>MAX_RECORDS){records_.pop_front();++dropped_by_capacity_;}
        return split?ObserveResult::NEW_SEGMENT:ObserveResult::RECORDED;
    }
    const std::deque<Record>& Records()const{return records_;}
    const std::vector<std::string>& Fields()const{return fields_;}
    const std::string& Source()const{return source_;}
    std::uint64_t TypeId()const{return type_id_;}
    std::uint64_t Observations()const{return observations_;}
    std::uint64_t UnobservedSequenceSteps()const{return gaps_;}
    std::uint64_t DroppedByCapacity()const{return dropped_by_capacity_;}
    // CSV exports only retained, actually observed samples. The sequence gap
    // count is an observation gap, NOT a network packet loss estimate.
    bool WriteCsv(std::ostream& stream)const {
        stream<<"source,type_id,observed_steady_us,observed_elapsed_ms,transport_sequence,unobserved_sequence_steps";
        for(const auto& name:fields_){stream<<',';Quote(stream,name);}
        stream<<'\n';
        if(records_.empty())return static_cast<bool>(stream);
        const auto first=records_.front().observed_at;
        for(const auto& record:records_) {
            Quote(stream,source_);
            const auto us=std::chrono::duration_cast<std::chrono::microseconds>(record.observed_at.time_since_epoch()).count();
            const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(record.observed_at-first).count();
            stream<<','<<type_id_<<','<<us<<','<<elapsed<<','<<record.sequence<<','<<record.unobserved_sequence_steps;
            for(double value:record.values)stream<<','<<value;
            stream<<'\n';
        }
        return static_cast<bool>(stream);
    }
private:
    static void Quote(std::ostream& out,const std::string& value) {
        out<<'"';for(char c:value){if(c=='"')out<<'"';out<<c;}out<<'"';
    }
    static bool Number(const std::string& text,double& output) {
        if(text.empty())return false;
        char* end=nullptr;errno=0;
        const double number=std::strtod(text.c_str(),&end);
        if(errno==ERANGE||end==text.c_str()||*end!='\0'||!std::isfinite(number))return false;
        output=number;return true;
    }
    static bool Flatten(const DataSnapshot& data,std::vector<std::string>& names,std::vector<double>& values) {
        for(const auto& field:data.fields) {
            if(field.kind!=ValueKind::SIGNED_INTEGER &&
               field.kind!=ValueKind::UNSIGNED_INTEGER &&
               field.kind!=ValueKind::FLOATING_POINT)continue;
            const bool array=!field.array_values.empty();
            const auto n=array?field.array_values.size():std::size_t{1};
            if(n>MAX_NUMERIC_FIELDS || names.size()>MAX_NUMERIC_FIELDS-n)return false;
            for(std::size_t i=0;i<n;++i){
                const auto& text=array?field.array_values[i]:field.value;
                double value=0;
                if(!Number(text,value))return false;
                names.push_back(field.name+(array?"["+std::to_string(i)+"]":""));
                values.push_back(value);
            }
        }
        return true;
    }
    std::string source_;
    std::uint64_t type_id_{0},observations_{0},gaps_{0},dropped_by_capacity_{0};
    std::vector<std::string> fields_;
    std::deque<Record> records_;
};
}
