#include "base/message_loop/message_pump_live.h"
#include "base/posix/eintr_wrapper.h"
#include "base/files/file_util.h"
#include "base/auto_reset.h"
#include <unistd.h>

namespace base {
namespace {
//Live555默认是10ms，在空闲时候也有明显的CPU消耗，我们改成5分钟
constexpr int64_t kMaxSchedulerGranularity = 300 * 1000000;  //五分钟
}
MessagePumpLive::MessagePumpLive()
    : keep_running_(true),
      in_run_(false),
      wakeup_pipe_in_(-1),
      wakeup_pipe_out_(-1),
      scheduler_(BasicTaskScheduler::createNew(kMaxSchedulerGranularity)),
      environment_(BasicUsageEnvironment::createNew(*scheduler_)) {
  if (!Init()) {
    NOTREACHED();
  }
}

MessagePumpLive::~MessagePumpLive() {
  environment_->reclaim();
  scheduler_.reset();
  if (wakeup_pipe_in_ >= 0) {
    if (IGNORE_EINTR(close(wakeup_pipe_in_)) < 0)
      DPLOG(ERROR) << "close";
  }
  if (wakeup_pipe_out_ >= 0) {
    if (IGNORE_EINTR(close(wakeup_pipe_out_)) < 0)
      DPLOG(ERROR) << "close";
  }
}

void MessagePumpLive::Run(Delegate *delegate) {
  scheduler_->setBackgroundHandling(wakeup_pipe_out_, SOCKET_READABLE, &MessagePumpLive::OnWakeUp, this);
  AutoReset<bool> auto_reset_keep_running(&keep_running_, true);
  AutoReset<bool> auto_reset_in_run(&in_run_, true);
  for (;;) {
    bool did_work = delegate->DoWork();
    if (!keep_running_)
      break;

    reinterpret_cast<BasicTaskScheduler0 *>(scheduler_.get())->SingleStep(1);
    if (!keep_running_)
      break;

    did_work |= delegate->DoDelayedWork(&delayed_work_time_);
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    did_work = delegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    if (delayed_work_time_.is_null()) {
      //SingleStep(0)表示按照555内部时间select，BasicTaskScheduler::createNew(kMaxSchedulerGranularity)
      reinterpret_cast<BasicTaskScheduler0 *>(scheduler_.get())->SingleStep();
    } else {
      TimeDelta delay = delayed_work_time_ - TimeTicks::Now();
      if (delay > TimeDelta()) {
        reinterpret_cast<BasicTaskScheduler0 *>(scheduler_.get())->SingleStep(delay.InMicroseconds());
      }
    }

    if (!keep_running_)
      break;
  }
  scheduler_->disableBackgroundHandling(wakeup_pipe_out_);
}

void MessagePumpLive::Quit() {
  DCHECK(in_run_) << "Quit was called outside of Run!";
  // Tell both Live555 and Run that they should break out of their loops.
  keep_running_ = false;
  ScheduleWork();
}

void MessagePumpLive::ScheduleWork() {
  char buf = 0;
  int nwrite = HANDLE_EINTR(write(wakeup_pipe_in_, &buf, 1));
  DPCHECK(nwrite == 1 || errno == EAGAIN) << "nwrite:" << nwrite;
}

void MessagePumpLive::ScheduleDelayedWork(const base::TimeTicks &delayed_work_time) {
  // We know that we can't be blocked on Wait right now since this method can
  // only be called on the same thread as Run, so we only need to update our
  // record of how long to sleep when we do sleep.
  delayed_work_time_ = delayed_work_time;
}

UsageEnvironment *MessagePumpLive::environment() const {
  return environment_;
}

bool MessagePumpLive::Init() {
  int fds[2];
  if (!base::CreateLocalNonBlockingPipe(fds)) {
    DLOG(ERROR) << "pipe() failed, errno: " << errno;
    return false;
  }
  wakeup_pipe_out_ = fds[0];
  wakeup_pipe_in_ = fds[1];
  return true;
}

void MessagePumpLive::OnWakeUp(void *clientData, int mask) {
  // Remove and discard the wakeup byte.
  auto that = static_cast<MessagePumpLive *>(clientData);
  char buf;
  int nread = HANDLE_EINTR(read(that->wakeup_pipe_out_, &buf, 1));
  DCHECK_EQ(nread, 1);
}

}
