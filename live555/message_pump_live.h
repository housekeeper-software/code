#ifndef RTSP_BASE_MESSAGE_PUMP_LIVE_H_
#define RTSP_BASE_MESSAGE_PUMP_LIVE_H_

#include <memory>
#include <string>
#include "base/macros.h"
#include "build/build_config.h"
#include "base/message_loop/message_pump.h"

#include <BasicUsageEnvironment.hh>

namespace rtsp {
class MessagePumpLive : public base::MessagePump {
public:
 MessagePumpLive();
 virtual ~MessagePumpLive();
 void Run(Delegate *delegate) override;
 void Quit() override;
 void ScheduleWork() override;
 void ScheduleDelayedWork(const base::TimeTicks &delayed_work_time) override;
 static UsageEnvironment *env();
private:
 bool Init();
 static void OnWakeUp(void *clientData, int mask);
 void DoRunLoop();
 // We may make recursive calls to Run, so we save state that needs to be
 // separate between them in this structure type.
 struct RunState;

 RunState *state_;
 // This is the time when we need to do delayed work.
 base::TimeTicks delayed_work_time_;

 int wakeup_pipe_read_;
 int wakeup_pipe_write_;
 bool processed_io_events_ = false;
 std::unique_ptr<TaskScheduler> scheduler_;
 UsageEnvironment *env_;
 DISALLOW_COPY_AND_ASSIGN(MessagePumpLive);
};
}
#endif //RTSP_BASE_MESSAGE_PUMP_LIVE_H_