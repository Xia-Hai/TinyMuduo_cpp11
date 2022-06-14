#include "Socket.h"
#include <unistd.h>
#include "Logger.h"
#include <sys/types.h>
#include <sys/socket.h>
#include "InetAddress.h"
#include <strings.h>
#include <netinet/tcp.h>

Socket::~Socket() {
    ::close(sockfd_);
}


void Socket::bindAddress(const InetAddress &localaddr) {
    if (0 != ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in))) {
        LOG_FATAL("bind sockfd: %d fail \n", sockfd_);
    }
}

void Socket::listen() {
    if (0 != ::listen(sockfd_, 1024)) {
        LOG_FATAL("listen fd %d fail \n", sockfd_);
    }
}

int Socket::accept(InetAddress *peeraddr) {
    sockaddr_in addr;
    bzero(&addr, sizeof addr);
    socklen_t len = sizeof addr;
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    // connfd 需要设置为non-blocking
    if (connfd >= 0) {
        peeraddr->setSockAddr(addr);
    }
    else {
        LOG_FATAL("accept fd %d fail \n", sockfd_);
    }

    return connfd;
}
// 半关闭，只关闭写端
void Socket::shutdownWrite() {
    if (::shutdown(sockfd_, SHUT_WR) < 0) {
        LOG_ERROR("shutdownWrite error \n");
    }
}
// 相当于关闭nagle算法
void Socket::setTcpNoDelay(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}

void Socket::setReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

void Socket::setReusePort(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

void Socket::setKeepAlive(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}