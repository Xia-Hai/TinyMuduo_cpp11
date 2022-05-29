#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"


EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseloop, const std::string &nameArg) 
    : baseloop_(baseloop),
    name_(nameArg),
    start_(false),
    numThreads_(0),
    next_(0)
{

}

EventLoopThreadPool::~EventLoopThreadPool() {
    // 线程里面的loop都是栈上的对象，不需要手动释放
}


void EventLoopThreadPool::start(const ThreadInitCallback &cb) {
    start_ = true;
    for (int i = 0; i < numThreads_; ++i) {
        char buf[name_.size() + 32]; // 给底层线程命名
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        // 创建EventloopThread
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));

        loops_.push_back(t->startLoop()); // 底层创建线程，绑定一个新的EventLoop，并返回loop的地址
    }

    // 如果没有设置过numThreads_
    if (numThreads_ == 0 && cb) {
        cb(baseloop_); // 只有一个baseloop
    }
}
// 如果工作在多线程中，baseloop默认以轮训的方式分配channel给subloop
std::vector<EventLoop*> EventLoopThreadPool::getAllloops() {
    if (loops_.empty()) {
        return std::vector<EventLoop*>(1, baseloop_);
    }
    else {
        return loops_;
    }
}

EventLoop *EventLoopThreadPool::getNextLoop() {
    EventLoop *loop = baseloop_;
    if (!loops_.empty()) {
        loop = loops_[next_]; // next_初始化为0
        next_++;
        if (next_ >= loops_.size()) {
            next_ = 0; // (next_++) %= loops_.size()
        }
    }
    return loop;
}