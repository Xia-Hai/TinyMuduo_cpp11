#pragma once

/**
 * noncopyable 的子类不能调用拷贝构造和拷贝赋值
 * 
 */
class noncopyable {
public:
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator=(const noncopyable &) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};