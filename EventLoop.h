#pragma once
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include "Timestamp.h"
#include "noncopyable.h"
#include "CurrentThread.h"

/**
 * 事件循环，主要包含两个模块  Channel 和 Poller（epoll的抽象）
 * 
 */

class Channel;
class Poller;
class Timestamp;

class EventLoop : noncopyable {
public:
    using Functor = std::function<void()>; // 定义回调的类型
    EventLoop();
    ~EventLoop();
    // 开启和退出事件循环
    void loop();
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }
    // 在当前loop中执行cb
    void runInLoop(Functor cb);
    // 把cb放在队列中，唤醒loop所在线程，执行cb
    void queueInLoop(Functor cb);

    void wakeup(); // mainLoop 用来唤醒subLoop
    
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);

    bool hasChannel(Channel *channel);
    // 判断loop是否在自己的线程中
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

    void handleRead(); // 处理wakeup
    void doPendingFunctors(); // 执行回调


private:
    
    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_; // 原子操作，通过CAS实现
    std::atomic_bool quit_; // 标志退出loop循环

    const pid_t threadId_; // 记录当前loop所在线程的ID
    Timestamp pollReturnTime_; // Poller返回发生事件的channels的时间点

    std::unique_ptr<Poller> poller_; // EventLoop管理的Poller

    int wakeupFd_; // 主要作用：当主循环获取到一个新用户的channel，通过轮询算法，唤醒subloop来处理（eventfd调用来创建）

    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;
   // Channel *currentActiveChannel_;

    std::atomic_bool callingPendingFunctors_; // 标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_; // 存储Loop需要执行的所有回调操作
    std::mutex mutex_; // 互斥锁，保护上面vector容器的线程安全

};