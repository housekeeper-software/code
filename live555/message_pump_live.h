#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIVE_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIVE_H_

#include <memory>
#include <string>
#include "base/macros.h"
#include "build/build_config.h"
#include "base/files/file_path.h"
#include "base/synchronization/lock.h"
#include "base/message_loop/message_pump.h"
#include <BasicUsageEnvironment.hh>

namespace base {
class MessagePumpLive : public MessagePump {
public:
 MessagePumpLive();
 virtual ~MessagePumpLive();
 void Run(Delegate *delegate) override;
 void Quit() override;
 void ScheduleWork() override;
 void ScheduleDelayedWork(const base::TimeTicks &delayed_work_time) override;
 UsageEnvironment *environment() const;
private:
 bool Init();
 static void OnWakeUp(void *clientData, int mask);
 // This flag is set to false when Run should return.
 bool keep_running_;
 // This flag is set when inside Run.
 bool in_run_;
 int wakeup_pipe_in_;
 int wakeup_pipe_out_;
 TimeTicks delayed_work_time_;
 std::unique_ptr<TaskScheduler> scheduler_;
 UsageEnvironment *environment_;
 DISALLOW_COPY_AND_ASSIGN(MessagePumpLive);
};
}
#endif //BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIVE_H_