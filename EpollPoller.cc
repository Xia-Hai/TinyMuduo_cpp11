#include "EpollPoller.h"
#include "Channel.h"
#include "Logger.h"
#include <errno.h>
#include <unistd.h>
#include <string.h>


const int kNew = -1; // 还没有在Poller中添加添加channel（对应channel的index成员变量）
const int kAdded = 1; // 代表三种不同的状态
const int kDeleted = 2;

EpollPoller::EpollPoller(EventLoop *loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)
{
    if (epollfd_ < 0) {
        LOG_FATAL("epoll_create error: %d \n", errno);
    }
}

EpollPoller::~EpollPoller() {
    ::close(epollfd_);
}

Timestamp EpollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
    // 用LOG_INFO会降低效率
    LOG_INFO("func = %s fd total count: %d\n", __FUNCTION__, (int)channels_.size());
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno; // 多线程考虑
    Timestamp now(Timestamp::now());
    if (numEvents > 0) {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size()) { // 当前vector满了需要扩容
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0) {
        LOG_DEBUG("%s timeout \n", __FUNCTION__);
    }
    else {
        if (saveErrno != EINTR) {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() error\n");
        }
    }
    return now; // 返回之前存下来的Timestamp
}

// 调用顺序为 Channel->update, remove ---> EventLoop->update, remove---> Poller->update, remove
/**
 *                  Eventloop
 *         ChannelList       Poller
 *                          ChannelMap<fd, channel*>    
 */
void EpollPoller::updateChannel(Channel *channel) {
    const int index = channel->index();
    LOG_INFO("func = %s fd = %d events = %d index = %d \n", __FUNCTION__, channel->fd(), channel->event(), index);

    if (index == kNew || index == kDeleted) {
        if (index == kNew) {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else { // 已经注册过了
        int fd = channel->fd();
        if (channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}
// 从Poller中移除channel，也就是从epollfd上删除fd
void EpollPoller::removeChannel(Channel *channel) {
    int fd = channel->fd();
    int index = channel->index();
    LOG_INFO("func = %s fd = %d events = %d index = %d \n", __FUNCTION__, channel->fd(), channel->event(), index);
    channels_.erase(fd);

    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
        channel->set_index(kDeleted);
    }
    channel->set_index(kNew);
}

    // 填写活跃的连接
void EpollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const {
    for (int i = 0; i < numEvents; i++) {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}
    // 更新channel通道
void EpollPoller::update(int operation, Channel *channel) {
    epoll_event event;
    memset(&event, 0, sizeof event);
    int fd = channel->fd();
    event.events = channel->event();
    event.data.fd = fd;
    event.data.ptr = channel; // 为fillActiveChannels()函数服务通过fd找到channel
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            LOG_ERROR("epoll_ctl del error: %d", errno);
        }
        else {
            LOG_FATAL("epoll_ctl add/mod error: %d", errno);
        }
    }
}