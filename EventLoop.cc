#include "EventLoop.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"



// 防止一个线程创建多个EventLoop            __thread相当于thread_local
__thread EventLoop *t_loopInThisThread = nullptr;

const int kPollTimeMs = 10000; // 默认的Poller IO复用接口的超时时间

int creatEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); // 用来通知subloop
    if (evtfd < 0) {
        LOG_FATAL("eventfd error: %d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop() 
    : looping_(false),
    quit_(false),
    callingPendingFunctors_(false),
    threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),
    wakeupFd_(creatEventfd()),
    wakeupChannel_(new Channel(this, wakeupFd_)) 
{
    LOG_DEBUG("EventLoop Create %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread) {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else {
        t_loopInThisThread = this;
    }

    // 设置wakeupfd的事件类型，和唤醒后的回调操作（唤醒subloop）
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个EventLoop都会监听wakeupChannel的EPOLLIN事件
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping \n", this);
    while (!quit_) {
        activeChannels_.clear(); // 存放的发生事件的channel
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_) {
            // Poller监听发生事件的channel上报给EventLoop，通知channel处理相关的事件
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop循环需要处理的回调操作
        /**
         * mainloop唤醒subloop 来执行mainloop注册的回调操作
         */
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping \n", this);
    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup(); // 在其他线程中调用quit，在subloop中调用了mainloop的quit
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    }
    else {
        // 唤醒loop所在线程来执行cb
        queueInLoop(cb);
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    // 唤醒相应需要执行回调操作的线程, callingPendingFunctors为true说明正在执行回调，需要在此注册可读事件，
    // 防止当前回调执行完没法执行新加入的回调函数
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8 \n", n);
    }
}
// 想wakeupfd写一个数据, wakeupChannel就会发生读事件被唤醒
void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERROR("EventLoop::wakeup() writes %lu instead of 8 \n", n);
    }
}

void EventLoop::updateChannel(Channel *channel) {
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel) {
    poller_->removeChannel(channel);
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    // 防止主loop添加事件时阻塞
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }
    for (const Functor &functor: functors) {
        functor(); //执行当前loop的回调操作
    }
    callingPendingFunctors_ = false;

}