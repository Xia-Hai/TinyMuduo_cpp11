#include <functional>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <errno.h>
#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

static EventLoop *CheckLoop(EventLoop *loop) {
    if (!loop) {
        LOG_FATAL("%s:%s:%d TcpConnection loop is null \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}


TcpConnection::TcpConnection(EventLoop *loop, const std::string &name, int sockfd, const InetAddress &localAddr, const InetAddress &peerAddr)
    : loop_(CheckLoop(loop)),
    name_(name),
    state_(kConnnecting),
    reading_(true),
    socket_(new Socket(sockfd)),
    channel_(new Channel(loop, sockfd)),
    localAddr_(localAddr),
    peerAddr_(peerAddr),
    highWaterMark_(64 * 1024 * 1024) // 64M
{
    // 设置channel的回调函数，Poller通知Channel后，Channel调用相应的回调函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd = %d \n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true); // 启动长连接
}
TcpConnection::~TcpConnection() {
    // 只有栈空间，使用了智能指针
    LOG_INFO("TcpConnection::dtor[%s] fd=%d\n", name_.c_str(), channel_->fd());
}

void TcpConnection::handleRead(Timestamp receiveTime) {
    int saveErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    if (n > 0) {
        // 已经建立连接的用户有可读事件发生了，调用用户传入的回调操作
        messageCallback_(/*std::enable_shared_from_this<TcpConnection>::*/shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0) {
        handleClose();
    }
    else {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead\n");
        handleError();
    }

}
void TcpConnection::handleWrite() {
    if (channel_->isWriting()) {
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if (n > 0) {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting(); // 发送完成取消注册可写事件
                if (writeCompleteCallback_) {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, /*std::enable_shared_from_this<TcpConnection>::*/shared_from_this()));
                }
                if (state_ == kDisconnecting) {
                    // 当前正在关闭
                    shutdownInLoop();
                }
            }
        }
        else {
            LOG_ERROR("TcpConnection::handleWrite\n");
        }
    }
    else {
        LOG_ERROR("TcpConnection fd = %d is dowm, no more write \n", channel_->fd());
    }
}

void TcpConnection::handleClose() {
    LOG_INFO("fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); // 执行连接关闭的回调
    closeCallback_(connPtr);
}

void TcpConnection::handleError() {
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        err = errno;
    }
    else {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s -SO_ERROR:%d \n", name_.c_str(), err);
}


void TcpConnection::send(const std::string buf) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf.c_str(), buf.size());
        }
        else {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}
// 发送数据，应用写的快，内核发送慢，需要把待发送的数据写入缓冲区，而且设置了水位回调
void TcpConnection::sendInLoop(const void *data, size_t len) {
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if (state_ == kDisconnected) {
        LOG_ERROR("disconnected, give up send\n");
    }
    // channel第一次开始写数据，而且缓冲区没有发送数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0) {
            remaining = len - nwrote; // 还需要发送的数据
            // 如果一次就发送完成了，就不用给channel设置EpoLLOUT事件了
            if (remaining == 0 && writeCompleteCallback_) {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                LOG_ERROR("TcpConnection::sendInLoop error\n");
                if (errno == EPIPE || errno == ECONNRESET) { // SIGPIPE RESET
                    faultError = true; // 有错误发生
                }
            }

        }
    }
    // 没有一次发送完成，需要把剩余的数据保存到缓冲区中，然后给channel注册epollout事件，poller发现发送缓冲区有空间，触发相应的回调函数
    if (!faultError && remaining > 0) {
        size_t oldLen = outputBuffer_.readableBytes(); // 目前可以发送的长度
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}

// 连接建立
void TcpConnection::connectEstablelished() {
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); // 注册EPOLLIN

    // 新链接建立执行回调
    connectionCallback_(shared_from_this());
} // 只能掉调用一次


// 连接销毁
void TcpConnection::connectDestroyed() {
    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll();

        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

void TcpConnection::shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, shared_from_this()));
    }
}

void TcpConnection::shutdownInLoop() {
    if (!channel_->isReading()) {
        // 当前outputBuffer中的数据已经发生完成了
        socket_->shutdownWrite();
    }
}