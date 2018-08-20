#pragma once

#include "macros.hpp"
#include "random.hpp"

#include <atomic>
#include <cassert>
#include <chrono>

namespace ccutils {

// shell: getconf LEVEL1_DCACHE_LINESIZE
static constexpr size_t CACHELINE_SIZE = 64;
static constexpr size_t MAX_WAIT_ITERS = 0x10000;
static constexpr size_t MIN_BACKOFF_ITERS = 32;
static constexpr size_t MAX_BACKOFF_ITERS = 1024;

void bindThisThreadToCore(size_t cpu) {
    pthread_t thisThread = pthread_self();
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(cpu, &cpuSet);
    const auto res = pthread_setaffinity_np(thisThread, sizeof(cpuSet), &cpuSet);
    if (res != 0) {
        std::cout << res << std::endl;
        std::cout << cpu << std::endl;
    }
    assert(res == 0);
}

class Spinlock {
private:
    alignas(CACHELINE_SIZE) std::atomic_bool locked = {false};

    ALWAYS_INLINE static void cpuRelax() { asm("pause"); }

    ALWAYS_INLINE static void yieldSleep() {
        // Don't yield but sleep to ensure that the thread is not
        // immediately run again in case scheduler's run queue is empty
        using namespace std::chrono;
        std::this_thread::sleep_for(500us);
    }

    ALWAYS_INLINE static void backoffExp(size_t& curMaxIters) {
        assert(curMaxIters > 0);

        thread_local std::uniform_int_distribution<size_t> dist;
        thread_local auto gen = randomSeeded<std::minstd_rand>();
        const size_t spinIters = dist(gen, decltype(dist)::param_type{0, curMaxIters});

        curMaxIters = std::min(2 * curMaxIters, MAX_BACKOFF_ITERS);
        for (size_t i = 0; i < spinIters; i++) cpuRelax();
    }

    ALWAYS_INLINE void waitUntilLockIsFree() const {
        size_t numIters = 0;

        while (locked.load(std::memory_order_relaxed)) {
            if (numIters < MAX_WAIT_ITERS) {
                cpuRelax();
                numIters++;
            } else
                yieldSleep();
        }
    }

public:
    ALWAYS_INLINE void lock() {
        size_t curMaxIters = MIN_BACKOFF_ITERS;

        while (true) {
            // Not strictly required but doesn't hurt
            waitUntilLockIsFree();

            if (locked.exchange(true, std::memory_order_acquire) == true)
                backoffExp(curMaxIters);  // Couldn't acquire lock => back-off
            else
                break;  // Acquired lock => done
        }
    }

    ALWAYS_INLINE void unlock() { locked.store(false, std::memory_order_release); }

};  // namespace ccutils

static_assert(sizeof(Spinlock) == CACHELINE_SIZE, "");

}  // namespace ccutils
