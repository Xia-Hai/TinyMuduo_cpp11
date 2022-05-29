#pragma once
#include "noncopyable.h"
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <unordered_map>
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "EventLoopThreadPool.h"
#include "TcpConnection.h"
#include "Callbacks.h"

/**
 * 对外服务器编程使用的类
 * 
 */

class TcpServer : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option{
        kNoReusePort,
        kReusePort
    };

    TcpServer(EventLoop *loop, const InetAddress &listenaddr, const std::string &nameArg, Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback &cb) {
        threadInitCallback_ = cb;
    }
    
    void setConnectionCallback(const ConnectionCallback &cb) {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback &cb) {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
        writeCompleteCallback_ = cb;
    }

    void setThreadNum(int numThreads);

    void start(); // 开启服务监听



private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnetionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_; // baseloop用户自己定义
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_; // 监听的新的连接，运行在mainloop
    std::shared_ptr<EventLoopThreadPool> threadPool_; // one loop per thread
    
    ConnectionCallback connectionCallback_; // 新链接的回调
    MessageCallback messageCallback_; // 有读写事件发生的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调

    ThreadInitCallback threadInitCallback_; // loop 线程初始化回调

    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_; // 保存所有的连接
};