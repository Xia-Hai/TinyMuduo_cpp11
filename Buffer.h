#pragma once

#include <string>
#include <vector>
#include <algorithm>

/**
 * 底层缓冲区类型
 * 
 */
class Buffer {
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + kInitialSize),
        readerIndex_(kCheapPrepend),
        writerIndex_(kCheapPrepend)
    {
    }

    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    size_t prependableBytes() const { return readerIndex_; }
    // 返回缓冲区中可读数据的起始地址
    const char *peek() const {
        return begin() + readerIndex_;
    }

    void retrieve(size_t len) {
        if (len < readableBytes()) {
            readerIndex_ += len; // 将readIndex往后移动，此时只读取了部分数据
        }
        else {
            retrieveAll();
        }
    }

    void retrieveAll() {
        readerIndex_ = writerIndex_ = kCheapPrepend; // 全部清零
    }
    // 把buffer数据转成string返回
    std::string retrieveAllAsString() {
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len) {
        std::string res(peek(), len);
        retrieve(len); // 对缓冲区进行复位操作
        return res;
    }
    // buffer_.size() - writerIndex_ < len 说明空间不够，需要扩容
    void ensureWriteableBytes(size_t len) {
        if (writableBytes() < len) {
            makeSpace(len);
        }
    }

    void append(const char *data, size_t len) {
        ensureWriteableBytes(len);
        std::copy(data, data + len, begin() + writerIndex_); //TODO封装writebegin() 方法
        // 更新游标
        writerIndex_ += len;
    }
    // 从fd上读取数据
    ssize_t readFd(int fd, int *savedError);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int *saveError);
private:
    char *begin() { return &*buffer_.begin(); } // vector首元素的地址
    const char *begin() const { return &*buffer_.begin(); }
    // 扩容操作
    void makeSpace (size_t len) {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
            buffer_.resize(writerIndex_ + len);
        }
        else {
            // 当前可读的长度
            size_t readAble = readableBytes();
            // 把数据往前拷贝
            std::copy(buffer_.begin() + readerIndex_, buffer_.begin() + writerIndex_, buffer_.begin() + kCheapPrepend);
            // 更新游标
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readAble;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};