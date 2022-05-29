#include "Channel.h"
#include "Logger.h"
#include "EventLoop.h"
#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; // 相当于读事件
const int Channel::kWriteEvent = EPOLLOUT; // 相当于写事件

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop),
    fd_(fd),
    events_(0),
    revents_(0),
    index_(-1),
    tied_(false) {
}

Channel::~Channel() {

}

void Channel::tie(const std::shared_ptr<void> &obj) {
    tie_ = obj; // 通过shared_ptr 初始化 weak_ptr
    tied_ = true;
}

/**
 * 改变Channel所表示的fd事件后，update负责在Poller里面更改相应的事件（epoll_ctl)
 * EventLoop ---> Channel
 * EventLoop ---> Poller
 * 
 */
void Channel::update() {
    // 通过Channel对应的EventLoop调用Poller
    //add code
    loop_->updateChannel(this);
}

// 在channel 所属的EventLoop中删除当前的Channel
void Channel::remove() {
    // 调用EventLoop中的方法
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime) {
    if (tied_) {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
    }
    else {
        handleEventWithGuard(receiveTime);
    }
}

// 根据Poller通知的具体发生事件，Channel进行分情况操作
void Channel::handleEventWithGuard(Timestamp receiveTime) {
    // 分情况处理
    LOG_INFO("Channel handleEvent revents: %d\n", revents_);
    
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) {
            closeCallback_();
        }
    }
    
    if (revents_ & EPOLLERR) {
        if (errorCallback_) {
            errorCallback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI)) {
        if (readCallback_) {
            readCallback_(receiveTime);
        }
    }
    
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) {
            writeCallback_();
        }
    }

}