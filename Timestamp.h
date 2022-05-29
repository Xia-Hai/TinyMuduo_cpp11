#pragma once
#include <iostream>
/**
 * 时间戳
 * 
 */

class Timestamp {
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondSinceEpoch);
    static Timestamp now();
    std::string toString() const; // 只读方法

private:
    int64_t microSecondSinceEpoch_;
};