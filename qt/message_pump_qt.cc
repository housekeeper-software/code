#include "qt/message_pump_qt.h"
#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_for_ui.h"

namespace qt {

namespace {

// Return a timeout suitable for the glib loop, -1 to block forever,
// 0 to return right away, or a timeout in milliseconds from now.
int GetTimeIntervalMilliseconds(const base::TimeTicks &delayed_work_time) {
  if (delayed_work_time.is_null())
    return -1;

  // Be careful here.  TimeDelta has a precision of microseconds, but we want a
  // value in milliseconds.  If there are 5.5ms left, should the delay be 5 or
  // 6?  It should be 6 to avoid executing delayed work too early.
  double timeout =
      ceil((delayed_work_time - base::TimeTicks::Now()).InMillisecondsF());

  // Range check the |timeout| while converting to an integer.  If the |timeout|
  // is negative, then we need to run delayed work soon.  If the |timeout| is
  // "overflowingly" large, that means a delayed task was posted with a
  // super-long delay.
  return timeout < 0 ? 0 :
         (timeout > std::numeric_limits<int>::max() ?
          std::numeric_limits<int>::max() : static_cast<int>(timeout));
}

}  // anonymous namespace

MessagePumpForUIQt::MessagePumpForUIQt()
    : scheduler_([this]() {
  handleScheduledWork();
}) {
  LOG(INFO) << __func__;
}

MessagePumpForUIQt::~MessagePumpForUIQt() {
  LOG(INFO) << __func__;
}

void MessagePumpForUIQt::Run(Delegate *) {
  LOG(INFO) << __func__;
  // This is used only when MessagePumpForUIQt is used outside of the GUI thread.
  NOTIMPLEMENTED();
}

void MessagePumpForUIQt::Quit() {
  LOG(INFO) << __func__;
  // This is used only when MessagePumpForUIQt is used outside of the GUI thread.
  NOTIMPLEMENTED();
}

void MessagePumpForUIQt::ScheduleWork() {
  // NOTE: This method may called from any thread at any time.
  ensureDelegate();
  scheduler_.scheduleWork();
}

void MessagePumpForUIQt::ScheduleDelayedWork(const base::TimeTicks &delayed_work_time) {
  // NOTE: This method may called from any thread at any time.
  ensureDelegate();
  scheduler_.scheduleDelayedWork(GetTimeIntervalMilliseconds(delayed_work_time));
}

void MessagePumpForUIQt::ensureDelegate() {
  if (!delegate_) {
    LOG(INFO) << __func__;
    auto seq_manager = base::MessageLoopCurrent::GetCurrentSequenceManagerImpl();
    delegate_ = static_cast<base::sequence_manager::internal::ThreadControllerWithMessagePumpImpl *>(
        seq_manager->controller_.get());
  }
}

void MessagePumpForUIQt::handleScheduledWork() {
  bool more_work_is_plausible = delegate_->DoWork();

  base::TimeTicks delayed_work_time;
  more_work_is_plausible |= delegate_->DoDelayedWork(&delayed_work_time);

  if (more_work_is_plausible)
    return ScheduleWork();

  more_work_is_plausible |= delegate_->DoIdleWork();
  if (more_work_is_plausible)
    return ScheduleWork();

  ScheduleDelayedWork(delayed_work_time);
}
}