#include "Acceptor.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "InetAddress.h"
#include "Logger.h"
#include <unistd.h>
#include <errno.h>

static int createNonblocking() {
    int sockfd = ::socket(AF_INET, SOCK_NONBLOCK | SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0) {
        LOG_FATAL("%s:%s:%d listen socket create error \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport) 
    : loop_(loop),
    acceptSocket_(createNonblocking()), // socket()
    acceptChannel_(loop, acceptSocket_.fd()),
    listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr); // bind()

    // TcpServer::start() 启动 Acceptor::listen, 有新用户连接。要执行一个回调，connfd打包成channel唤醒一个subloop
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen() {
    listenning_ = true;
    acceptSocket_.listen(); // listen()
    acceptChannel_.enableReading(); // 注册可写事件
}

// listenFd有事件发生了，也就是有新用户连接了
void Acceptor::handleRead() {
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0) {
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd, peerAddr); // 轮询找到subloop唤醒分发当前的channel
        }
        else {
            ::close(connfd);
        }
    }
    else {
        LOG_ERROR("%s:%s:%d accept socket create error %d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE) {
            LOG_ERROR("打开文件描述符达到上限\n");
            // 可以开维护一个临时文件描述符，在出现EMFILE之后。使用临时文件描述符接收连接，然后再断开
        }
    }
}