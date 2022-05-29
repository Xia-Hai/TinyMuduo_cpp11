#pragma once

#include "noncopyable.h"
#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop *baseloop, const std::string &nameArg);

    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) {
        numThreads_ = numThreads;
    }

    void start(const ThreadInitCallback &cb = ThreadInitCallback());
    // 如果工作在多线程中，baseloop默认以轮训的方式分配channel给subloop
    EventLoop *getNextLoop();

    std::vector<EventLoop*> getAllloops();

    bool started() const { return start_; }
    const std::string name() const { return name_; }

private:
    EventLoop *baseloop_; // EventLoop loop最初创建的loop
    std::string name_;
    bool start_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop *> loops_;
};
