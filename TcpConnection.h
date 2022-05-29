#pragma once

#include "Buffer.h"
#include "Timestamp.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include <memory>
#include <string>
#include <atomic>


class EventLoop;
class Channel;
class Socket; // 前置声明

/**
 * TcpServer 通过Acceptor有一个新的连接，通过accept拿到连接的connfd
 * TcpConnection设置回调---->Channel----->Poller---->Channel调用回调操作
 * 
 */


class TcpConnection : noncopyable,
    public std::enable_shared_from_this<TcpConnection>
{
public:
    enum StateE { kDisconnected, kConnnecting, kConnected, kDisconnecting };

    TcpConnection(EventLoop *loop, const std::string &name, int sockfd, const InetAddress &localAddr, const InetAddress &peerAddr);
    ~TcpConnection();
    EventLoop *getloop() const { return loop_; }

    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }

    bool connected() const {return state_ == kConnected; }
    // 发送数据
    void send(const void *message, int len);
    // 断开连接
    void shutdown();
    // 设置回调操作
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark) {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }
    void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }

    // 连接建立
    void connectEstablelished(); // 只能掉调用一次
    // 连接销毁
    void connectDestroyed();

    void setState(StateE state) {
        state_ = state;
    }
    void send(const std::string buf);

private:
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();


    void sendInLoop(const void *message, size_t len);
    void shutdownInLoop();

    EventLoop *loop_; // 这里不是baseloop，TcpConnection都是在subloop中管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;
    // 这里和Acceptor类似，Acceptor在mainloop里面， TcpConnection在subloop里面
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_; // 新链接的回调
    MessageCallback messageCallback_; // 有读写事件发生的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;

    CloseCallback closeCallback_;

    size_t highWaterMark_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;

};