#pragma once
#include "kcf_tool/model/topic_plot_history.hpp"
#include <QPainter>
#include <QPaintEvent>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <limits>

namespace kcf_tool {
// View only. Rendering never drives Topic reads, controls or file I/O.
class TopicPlotWidget final : public QWidget {
public:
    explicit TopicPlotWidget(QWidget* parent=nullptr):QWidget(parent) {
        setMinimumHeight(160);
        setObjectName("topicPlotWidget");
        setToolTip("Observer-local monotonic time, latest-only Echo snapshots; missing samples are not interpolated.");
    }
    void SetHistory(const TopicPlotHistory* history,int field_index) {
        history_=history;field_=field_index;update();
    }
protected:
    void paintEvent(QPaintEvent*)override {
        QPainter painter(this);
        painter.fillRect(rect(),palette().base());
        painter.setPen(palette().text().color());
        const QRect bounds=rect().adjusted(55,12,-15,-28);
        if(bounds.width()<4||bounds.height()<4)return;
        painter.drawRect(bounds);
        if(!history_ || field_<0 || static_cast<std::size_t>(field_)>=history_->Fields().size() ||
           history_->Records().empty()) {
            painter.drawText(bounds,Qt::AlignCenter,"Choose a numeric field during Local Echo");
            return;
        }
        const auto& records=history_->Records();
        const auto first=records.front().observed_at;
        const double duration=std::chrono::duration<double>(records.back().observed_at-first).count();
        double lo=std::numeric_limits<double>::infinity(),hi=-std::numeric_limits<double>::infinity();
        for(const auto& r:records) {
            const double v=r.values[static_cast<std::size_t>(field_)];
            lo=std::min(lo,v);hi=std::max(hi,v);
        }
        if(!std::isfinite(lo)||!std::isfinite(hi))return;
        if(lo==hi) {const double pad=std::max(1.0,std::abs(lo)*0.05);lo-=pad;hi+=pad;}
        else {const double pad=(hi-lo)*0.05;lo-=pad;hi+=pad;}
        const auto point=[&](const TopicPlotHistory::Record& r) {
            const double t=std::chrono::duration<double>(r.observed_at-first).count();
            const double x=bounds.left()+(duration>0?t/duration:0.5)*bounds.width();
            const double y=bounds.bottom()-(r.values[static_cast<std::size_t>(field_)]-lo)/(hi-lo)*bounds.height();
            return QPointF(x,y);
        };
        painter.setPen(QPen(palette().highlight().color(),1.5));
        // A sequence gap means intermediate values were not observed. Never draw
        // a continuous line over such a gap; retain distinct sampled points.
        for(std::size_t i=1;i<records.size();++i) {
            if(records[i].unobserved_sequence_steps==0)
                painter.drawLine(point(records[i-1]),point(records[i]));
        }
        painter.setBrush(palette().highlight().color());
        for(const auto& r:records)painter.drawEllipse(point(r),2.0,2.0);
        painter.setPen(palette().text().color());
        painter.drawText(QRect(2,bounds.top()-6,50,20),Qt::AlignRight,QString::number(hi,'g',5));
        painter.drawText(QRect(2,bounds.bottom()-12,50,20),Qt::AlignRight,QString::number(lo,'g',5));
        painter.drawText(QRect(bounds.left(),bounds.bottom()+3,bounds.width(),20),
            Qt::AlignRight,QString::number(duration,'f',1)+" s observed");
    }
private:
    const TopicPlotHistory* history_{nullptr};
    int field_{-1};
};
}
