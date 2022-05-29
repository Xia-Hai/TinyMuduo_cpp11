#pragma once

#include <functional>
#include "noncopyable.h"
#include "Thread.h"
#include <mutex>
#include <condition_variable>
#include <string>
/**
 *  One Loop per thread
 * 
 */

class EventLoop;


class EventLoopThread : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
        const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop *startLoop(); // 开启循环
private:
    void threadFunc(); // 线程函数

    EventLoop *loop_;
    bool exiting_;

    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_; // 初始化的回调函数

};