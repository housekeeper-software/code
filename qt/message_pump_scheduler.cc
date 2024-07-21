#include "qt/message_pump_scheduler.h"
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QTimerEvent>

namespace qt {
MessagePumpScheduler::MessagePumpScheduler(std::function<void()> callback)
    : callback_(std::move(callback)) {}

void MessagePumpScheduler::scheduleWork() {
  QCoreApplication::postEvent(this, new QTimerEvent(0));
}

void MessagePumpScheduler::scheduleDelayedWork(int delay) {
  if (delay < 0) {
    killTimer(timer_id_);
    timer_id_ = 0;
  } else if (!timer_id_ ||
      delay < QAbstractEventDispatcher::instance()->remainingTime(timer_id_)) {
    killTimer(timer_id_);
    timer_id_ = startTimer(delay);
  }
}

void MessagePumpScheduler::timerEvent(QTimerEvent *ev) {
  Q_ASSERT(!ev->timerId() || timer_id_ == ev->timerId());
  killTimer(timer_id_);
  timer_id_ = 0;
  callback_();
}
}
