/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 * Description: 自旋锁
 */

#ifndef HCCL_SPIN_MUTEX_H
#define HCCL_SPIN_MUTEX_H

#include <mutex>
#include <atomic>

namespace hccl {

class SpinMutex {
public:
    SpinMutex() = default;
    ~SpinMutex() = default;
    // delete copy and move constructors and assign operators
    SpinMutex(SpinMutex const&) = delete;             // Copy construct
    SpinMutex(SpinMutex&&) = delete;                  // Move construct
    SpinMutex& operator=(SpinMutex const&) = delete;  // Copy assign
    SpinMutex& operator=(SpinMutex &&) = delete;      // Move assign
    void lock()
    {
        bool expected = false;
        while (!flag.compare_exchange_strong(expected, true)) {
            expected = false;
        }
    }
    void unlock()
    {
        flag.store(false);
    }
private:
    std::atomic<bool> flag = ATOMIC_VAR_INIT(false);
};
}  // namespace hccl
#endif  // HCCL_SPIN_MUTEX_H