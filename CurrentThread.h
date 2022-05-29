#pragma once

namespace CurrentThread {
    // __thread 修饰全局变量，每一个线程都有一份拷贝
    extern __thread int t_cachedTid;

    void cacheTid();

    inline int tid() {
        if (__builtin_expect(t_cachedTid == 0, 0)) {
            cacheTid();
        }
        return t_cachedTid;
    }
}