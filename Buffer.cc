#include "Buffer.h"
#include <errno.h>
#include <unistd.h>
#include <sys/uio.h> // readv 从多个缓冲区读

// 默认工作在LT模式，Buffer缓冲区是有大小的，但是fd上的流式数据不知道最终的大小
ssize_t Buffer::readFd(int fd, int *savedError) {
    char extrabuf[65536] = {0}; // 栈上的空间 64K
    struct iovec vec[2];
    const size_t writable = writableBytes(); // Buffer中剩余的可写入的大小，不一定能装下所有数据
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable; // 第一缓冲区的起始位置和长度

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf; // 额外提供的第二个缓冲区
    // 一次最多可以操作 64K-1+64K 数据
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        *savedError = errno;
    }
    else if (n <= writable) {
        writerIndex_ += n; // 更新游标
    }
    else {
        // extrabuf里面也写了数据
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable); // 吧extrabuf中的数据添加到缓冲区
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveError) {
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) {
        *saveError = errno;
    }
    return n;
}