#pragma once

#include <memory>
#include <functional>
#include "noncopyable.h"
#include "Timestamp.h"

/**
 * Channel 类理解为通道，封装了 sockfd和其感兴趣的事件event，如EPOLLIN、EPOLLOUT事件
 *                      还绑定了poller返回的事件
 * 
 */

class EventLoop; // 前置声明
//class Timestamp;

class Channel : noncopyable {
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();
    // fd得到Poller通知后处理事件，调用回调方法处理事件
    void handleEvent(Timestamp receiveTime);

    // 设置回调操作
    void setReadCallback(ReadEventCallback cb) {
        readCallback_ = std::move(cb);
    }
    void setWriteCallback(EventCallback cb) {
        writeCallback_ = std::move(cb);
    }
    void setCloseCallback(EventCallback cb) {
        closeCallback_ = std::move(cb);
    }
    void setErrorCallback(EventCallback cb) {
        errorCallback_ = std::move(cb);
    }

    // 防止channel被手动remove掉，channel还在执行回调函数
    void tie(const std::shared_ptr<void> &);

    int fd() const { return fd_; }
    int event() const { return events_; }
    void set_revents(int revt) { revents_ = revt; }


    // 设置相应fd注册事件的状态（调用Epoll_ctl)
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; } // 是否注册了事件
    bool isReading() const { return events_ & kReadEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }

    // for Poller ???
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    EventLoop *ownerLoop() const { return loop_; }
    
    void remove(); // 销毁功能
private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

private:
    static const int kNoneEvent;
    static const int kReadEvent; // 相当于读事件
    static const int kWriteEvent; // 相当于写事件

    EventLoop *loop_; // 事件循环
    const int fd_;    // fd， Poller监听的对象
    int events_;      // 注册的fd感兴趣的事件
    int revents_;     // Poller返回的具体发生的事件
    int index_;


    std::weak_ptr<void> tie_; // 使用weak_ptr 来监听跨线程的shared_ptr的生命周期
    bool tied_;

    // channel 通道里面能够知道fd最终发生的事件revents_,所以可以负责相应函数的回调
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};