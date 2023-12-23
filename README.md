将Live555消息循环整合到chromium MessageLoop中，以便使用chromium的TaskRunner的功能。
Live555 BasicTaskScheduler::createNew()默认 10ms为 select最大超时时间，在空闲时刻也会占用一定的CPU资源。我们改成比较长的时间，比如5分钟
或者一小时。总之，不管多长不会影响到消息泵的工作。
chromium各个版本 MessagePump架构变化不大，略加修改可以适应到各个版本。
经过测试，在空闲时 Live555线程 CPU占用为0，一路H264推流，CPU占用不超过4%左右。
其中：
~~~
UsageEnvironment *MessagePumpLive::environment() const;
可以向外部输出 Live555 UsageEnvironment对象。各个版本chromium导出接口的方式不同，75之前的版本，可以直接在MessageLoop导出。
75之后的版本，可以自己扩展 MessageLoopCurrent 导出，比如：


#if defined(USE_LIVE)
// ForIO extension of MessageLoopCurrent.
class BASE_EXPORT MessageLoopCurrentForLive : public MessageLoopCurrent {
public:
 // Returns an interface for the MessageLoopForIO of the current thread.
 // Asserts that IsSet().
 static MessageLoopCurrentForLive Get();

 // Returns true if the current thread is running a MessageLoopForIO.
 static bool IsSet();

 MessageLoopCurrentForLive *operator->() { return this; }

 UsageEnvironment *environment() const;

private:
 explicit MessageLoopCurrentForLive(
     sequence_manager::internal::SequenceManagerImpl *current)
     : MessageLoopCurrent(current) {}
};

UsageEnvironment *MessageLoopCurrentForLive::environment() const{
return static_cast<MessagePumpLive*>(current_->GetMessagePump())->environment();
}
#endif
~~~

使用：
~~~
在 Live555线程中：
 UsageEnvironment *env = base::MessageLoopCurrentForLive::Get()->environment();
~~~

其他：
~~~
可能还需要注册一种新的消息类型，比如 TYPE_LIVE，也可以用于替换  TYPE_IO/TYPE_UI。
~~~
