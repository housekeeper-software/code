#include "rtsp/base/message_pump_live.h"
#include "base/posix/eintr_wrapper.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include <unistd.h>

namespace rtsp {
namespace {
//Live555默认是10ms，在空闲时候也有明显的CPU消耗，我们改成5分钟
constexpr int64_t kMaxSchedulerGranularity = 300 * 1000000;  //五分钟

namespace {
class BasicUsageEnvironmentHolder {
public:
 BasicUsageEnvironmentHolder()
     : env_(nullptr) {}
 ~BasicUsageEnvironmentHolder() = default;
 void Set(UsageEnvironment *env) {
   env_ = env;
 }
 UsageEnvironment *Get() {
   return env_;
 }
private:
 UsageEnvironment *env_;
 DISALLOW_COPY_AND_ASSIGN(BasicUsageEnvironmentHolder);
};

base::LazyInstance<BasicUsageEnvironmentHolder>::DestructorAtExit
    g_live555_initializer = LAZY_INSTANCE_INITIALIZER;

}  // namespace


// Tracks the pump the most recent pump that has been run.
struct ThreadInfo {
 // The pump.
 MessagePumpLive *pump;

 // ID of the thread the pump was run on.
 base::PlatformThreadId thread_id;
};

// Used for accesing |thread_info|.
static base::LazyInstance<base::Lock>::Leaky live_thread_info_lock = LAZY_INSTANCE_INITIALIZER;

// If non-NULL it means a MessagePumpGlib exists and has been Run. This is
// destroyed when the MessagePump is destroyed.
ThreadInfo *thread_info = NULL;

void CheckThread(MessagePumpLive *pump) {
  base::AutoLock auto_lock(live_thread_info_lock.Get());
  if (!thread_info) {
    thread_info = new ThreadInfo;
    thread_info->pump = pump;
    thread_info->thread_id = base::PlatformThread::CurrentId();
  }
  DCHECK(thread_info->thread_id == base::PlatformThread::CurrentId()) <<
                                                                      "Running MessagePumpLive on two different threads; "
                                                                      "this is unsupported by Live!";
}

void PumpDestroyed(MessagePumpLive *pump) {
  base::AutoLock auto_lock(live_thread_info_lock.Get());
  if (thread_info && thread_info->pump == pump) {
    delete thread_info;
    thread_info = NULL;
  }
}
}

struct MessagePumpLive::RunState {
 Delegate *delegate;

 // Used to flag that the current Run() invocation should return ASAP.
 bool should_quit;

 // Used to count how many Run() invocations are on the stack.
 int run_depth;
};

MessagePumpLive::MessagePumpLive()
    : state_(nullptr),
      wakeup_pipe_read_(-1),
      wakeup_pipe_write_(-1),
      scheduler_(BasicTaskScheduler::createNew(kMaxSchedulerGranularity)),
      env_(BasicUsageEnvironment::createNew(*scheduler_)) {
  LOG(INFO) << __func__;
  if (!Init()) {
    NOTREACHED();
  } else {
    g_live555_initializer.Get().Set(env_);
    scheduler_->setBackgroundHandling(wakeup_pipe_read_,
                                      SOCKET_READABLE,
                                      &MessagePumpLive::OnWakeUp, this);
  }
}

MessagePumpLive::~MessagePumpLive() {
  LOG(INFO) << __func__ << ",begin";
#ifndef NDEBUG
  PumpDestroyed(this);
#endif
  scheduler_->disableBackgroundHandling(wakeup_pipe_read_);
  if (wakeup_pipe_read_ >= 0) {
    if (IGNORE_EINTR(close(wakeup_pipe_read_)) < 0)
      DPLOG(ERROR) << "close";
  }
  if (wakeup_pipe_write_ >= 0) {
    if (IGNORE_EINTR(close(wakeup_pipe_write_)) < 0)
      DPLOG(ERROR) << "close";
  }
  g_live555_initializer.Get().Set(nullptr);

  if (env_) {
    const Boolean result = env_->reclaim();
    if (result != True) {
      LOG(ERROR) << __func__ << ", Failed to reclaim BasicUsageEnvironment";
    }
    env_ = nullptr;
  }
  scheduler_.reset();
  LOG(INFO) << __func__ << ",end";
}

void MessagePumpLive::Run(Delegate *delegate) {
  LOG(INFO) << __func__ << ",enter[" << delegate << "]";
#ifndef NDEBUG
  CheckThread(this);
#endif
  RunState state;
  state.delegate = delegate;
  state.should_quit = false;
  state.run_depth = state_ ? state_->run_depth + 1 : 1;
  LOG(INFO) << __func__ << ",run depth[" << state.run_depth << "]";
  RunState *previous_state = state_;
  state_ = &state;

  DoRunLoop();

  state_ = previous_state;
  LOG(INFO) << __func__ << ",leave[" << delegate << "]";
}

void MessagePumpLive::DoRunLoop() {
  LOG(INFO) << __func__;
  for (;;) {
    bool did_work = state_->delegate->DoWork();
    if (state_->should_quit)
      break;

    did_work |= state_->delegate->DoDelayedWork(&delayed_work_time_);
    if (state_->should_quit)
      break;

    reinterpret_cast<BasicTaskScheduler0 *>(scheduler_.get())->SingleStep(1);
    did_work |= processed_io_events_;
    processed_io_events_ = false;
    if (state_->should_quit)
      break;

    if (did_work)
      continue;

    did_work = state_->delegate->DoIdleWork();
    if (state_->should_quit)
      break;

    if (did_work)
      continue;

    if (delayed_work_time_.is_null()) {
      //LOG(INFO) << __func__ << ",enter live555 internal loop";
      reinterpret_cast<BasicTaskScheduler0 *>(scheduler_.get())->SingleStep();
      //LOG(INFO) << __func__ << ",leave live555 internal loop";
    } else {
      base::TimeDelta delay = delayed_work_time_ - base::TimeTicks::Now();
      if (delay > base::TimeDelta()) {
        //LOG(INFO) << __func__ << ",enter live555 internal delayed loop,delay[" << delay.InMicroseconds() << "]";
        reinterpret_cast<BasicTaskScheduler0 *>(scheduler_.get())->SingleStep(delay.InMicroseconds());
        //LOG(INFO) << __func__ << ",leave live555 internal delayed loop";
      }
    }

    if (state_->should_quit)
      break;
  }
}

void MessagePumpLive::Quit() {
  LOG(INFO) << __func__;
  if (state_) {
    state_->should_quit = true;
    LOG(INFO) << __func__ << ",set quit flag";
    //这里不用唤醒线程
  } else {
    NOTREACHED() << "Quit called outside Run!";
  }
}

UsageEnvironment *MessagePumpLive::env() {
  LOG(INFO) << __func__;
  return g_live555_initializer.Get().Get();
}

void MessagePumpLive::ScheduleWork() {
  //LOG(INFO) << __func__;
  char buf = 0;
  int nwrite = HANDLE_EINTR(write(wakeup_pipe_write_, &buf, 1));
  DPCHECK(nwrite == 1 || errno == EAGAIN) << "nwrite:" << nwrite;
}

void MessagePumpLive::ScheduleDelayedWork(const base::TimeTicks &delayed_work_time) {
  // We know that we can't be blocked on Run()'s |timer_event| right now since
  // this method can only be called on the same thread as Run(). Hence we have
  // nothing to do here, this thread will sleep in Run() with the correct
  // timeout when it's out of immediate tasks.
  //LOG(INFO) << __func__;
}

bool MessagePumpLive::Init() {
  LOG(INFO) << __func__;
  int fds[2];
  if (!base::CreateLocalNonBlockingPipe(fds)) {
    DLOG(ERROR) << __func__ << ",pipe() failed, errno: " << errno;
    return false;
  }
  wakeup_pipe_read_ = fds[0];
  wakeup_pipe_write_ = fds[1];
  return true;
}

void MessagePumpLive::OnWakeUp(void *clientData, int mask) {
  //LOG(INFO) << __func__;
  // Remove and discard the wakeup byte.
  auto that = static_cast<MessagePumpLive *>(clientData);
  char buf;
  const int num_bytes = HANDLE_EINTR(read(that->wakeup_pipe_read_, &buf, 1));
  DCHECK_EQ(num_bytes, 1);
  that->processed_io_events_ = true;
}

}
