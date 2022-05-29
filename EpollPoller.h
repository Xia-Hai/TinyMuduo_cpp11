#pragma once

#include "Poller.h"
#include <vector>
#include <sys/epoll.h>
#include "Timestamp.h"

/**
 * 继承Poller，实现纯虚函数
 * epoll的使用
 * epoll_create 在构造和析构中
 * epoll_ctl (add, del, modify) 对应update和remove方法
 * epoll_wait 对应poll方法
 * 
 */
class Channel;

class EpollPoller : public Poller {
public:
    EpollPoller(EventLoop *loop);
    ~EpollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16;
    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新channel通道
    void update(int operation, Channel *channel);
    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};