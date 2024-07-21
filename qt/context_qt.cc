#include "qt/context_qt.h"
#include "base/message_loop/message_loop.h"
#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"
#include "base/run_loop.h"
#include "qt/message_pump_qt.h"

namespace qt {

std::unique_ptr<base::MessagePump> CreateMessagePumpFactory() {
  if (!base::MessageLoopCurrentForUI::IsSet()) {
    return std::make_unique<MessagePumpForUIQt>();
  }
  return nullptr;
}

void ContextQt::InitMessagePumpForUIFactory() {
  base::MessageLoop::InitMessagePumpForUIFactory(CreateMessagePumpFactory);
}

ContextQt::ContextQt()
    : main_message_loop_(new base::MessageLoopForUI()),
      run_loop_(new base::RunLoop()) {
  LOG(INFO) << __func__ << ",begin";
  // Once the MessageLoop has been created, attach a top-level RunLoop.
  run_loop_->BeforeRun();
  LOG(INFO) << __func__ << ",end";
}

ContextQt::~ContextQt() {
  LOG(INFO) << __func__ << ",begin";
  Shutdown();
  run_loop_->AfterRun();
  run_loop_.reset();
  main_message_loop_.reset();
  LOG(INFO) << __func__ << ",end";
}

void ContextQt::Shutdown() {
  LOG(INFO) << __func__ << ",begin";
  DCHECK(run_loop_);
  base::MessagePump::Delegate *delegate =
      static_cast<base::sequence_manager::internal::ThreadControllerWithMessagePumpImpl *>(
          run_loop_->delegate_);
  // Flush the UI message loop before quitting.
  while (delegate->DoWork()) {}
  LOG(INFO) << __func__ << ",end";
}
}