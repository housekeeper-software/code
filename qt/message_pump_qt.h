#ifndef QT_MESSAGE_PUMP_QT_H_
#define QT_MESSAGE_PUMP_QT_H_

#include "base/macros.h"
#include "base/message_loop/message_pump.h"
#include "qt/message_pump_scheduler.h"

namespace qt {
class MessagePumpForUIQt
    : public base::MessagePump {
public:
 MessagePumpForUIQt();
 ~MessagePumpForUIQt() override;
 void Run(Delegate *) override;
 void Quit() override;
 void ScheduleWork() override;
 void ScheduleDelayedWork(const base::TimeTicks &delayed_work_time) override;
private:
 void ensureDelegate();
 void handleScheduledWork();
 Delegate *delegate_ = nullptr;
 MessagePumpScheduler scheduler_;
};
}

#endif //QT_MESSAGE_PUMP_QT_H_