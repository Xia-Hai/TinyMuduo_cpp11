#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"
#include <strings.h>

static EventLoop *CheckLoop(EventLoop *loop) {
    if (!loop) {
        LOG_FATAL("%s:%s:%d mainloop is null \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenaddr, const std::string &nameArg, Option option) 
    : loop_(CheckLoop(loop)),
    ipPort_(listenaddr.toIpPort()),
    name_(nameArg),
    acceptor_(new Acceptor(loop, listenaddr, option == kReusePort)),
    threadPool_(new EventLoopThreadPool(loop, name_)),
    connectionCallback_(),
    messageCallback_(),
    started_(0),
    nextConnId_(1)
{
    // 新用户连接会执行回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));

}

void TcpServer::setThreadNum(int numThreads) {
    threadPool_->setThreadNum(numThreads);
}

// 随后调用loop开启时间循环
void TcpServer::start() {
    // 防止一个TcpServer被start多次
    if (started_++ == 0) {
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 有一个新的客户端连接，accpetor会调用这个回调操作
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
    // 轮询算法，选择一个subloop来管理当前的channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d', in", ipPort_.c_str(), nextConnId_);
    ++nextConnId_; // 只有mainloop使用nextConnId 不涉及线程安全的问题
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConneciont [%s] - new connection [%s] from %s \n",
        name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取绑定的本机的IP和PORT

    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrLen = sizeof local;

    if (::getsockname(sockfd, (sockaddr*)&local, &addrLen) < 0) {
        LOG_ERROR("socket::getsockname error\n");
    }
    InetAddress localAddr(local);

    // 根据连接成功的sockfd，创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));

    connections_[connName] = conn;

    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablelished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
    loop_->runInLoop(std::bind(&TcpServer::removeConnetionInLoop, this, conn));

}


void TcpServer::removeConnetionInLoop(const TcpConnectionPtr &conn) {
    LOG_INFO("TcpServet::removeConnectionInLoop [%s] - connection %s \n", name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getloop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}

TcpServer::~TcpServer() {
    // 没看懂TTTT
    for (auto &item : connections_) {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getloop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}