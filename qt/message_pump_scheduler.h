#ifndef QT_MESSAGE_PUMP_SCHEDULER_H_
#define QT_MESSAGE_PUMP_SCHEDULER_H_

#include "base/macros.h"
#include <QtCore/qobject.h>
#include <functional>

namespace qt {

class MessagePumpScheduler : public QObject {
Q_OBJECT
public:
 MessagePumpScheduler(std::function<void()> callback);
 void scheduleWork();
 void scheduleDelayedWork(int delay);
protected:
 void timerEvent(QTimerEvent *ev) override;
private:
 int timer_id_ = 0;
 std::function<void()> callback_;
};
}

#endif //QT_MESSAGE_PUMP_SCHEDULER_H_