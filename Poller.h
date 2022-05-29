#pragma once
#include <vector>
#include <unordered_map>
#include "noncopyable.h"
#include "Timestamp.h"

/**
 * 主要是用于多路事件的IO复用模块
 * 
 */

class Channel;

class EventLoop;

class Poller : noncopyable {
public:
    using ChannelList = std::vector<Channel*>;
    Poller(EventLoop *loop);
    virtual ~Poller() = default;
    // 给所用IO复用保留统一个接口
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    bool hasChannel(Channel *channel) const; // 判断channel是否在当前Poller中
    // EventLoop 可以通过这个接口获取IO复用的具体实现
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    // map中的key为sockfd，value为sockfd所属的参数类型
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;
private:
    EventLoop *ownerLoop_; // Poller所属的Eventloop
};