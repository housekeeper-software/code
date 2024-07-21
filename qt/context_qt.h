#ifndef QT_CONTEXT_QT_H_
#define QT_CONTEXT_QT_H_

#include "base/macros.h"
#include <memory>

namespace base {
class RunLoop;
class MessageLoop;
}
/*
/////////////////////////////////////////////////////////
int main(int argc, char *argv[]) {
  qt::ContextQt::InitMessagePumpForUIFactory();
  {
    app::MainApp app(argc, argv);
    app.Init();
    int res = app::MainApp::exec();
    app.UnInit();
  }
}
//////////////////////////////////////////////////////////
namespace {
void QuitApplication() {
  QTimer::singleShot(0, qApp, SLOT(quit()));
}
}

class MainApp {
public:
 MainApp(int &argc, char **argv)
     : QApplication(argc, argv),
       task_runner_(base::ThreadTaskRunnerHandle::Get()),
       weak_factory_(this) {
 }
 ~MainApp() {
   weak_factory_.InvalidateWeakPtrs();
 }
 void Quit() {
   task_runner_->PostTask(FROM_HERE,
                          base::BindOnce(&QuitApplication));
 }
private:
 qt::ContextQt context_qt_;
 scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
 base::WeakPtrFactory<MainApp> weak_factory_;
};
*/

namespace qt {
class ContextQt {
public:
 ContextQt();
 ~ContextQt();
 static void InitMessagePumpForUIFactory();
private:
 void Shutdown();
 std::unique_ptr<base::MessageLoop> main_message_loop_;
 std::unique_ptr<base::RunLoop> run_loop_;
 DISALLOW_COPY_AND_ASSIGN(ContextQt);
};
}

#endif //QT_CONTEXT_QT_H_