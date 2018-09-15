#pragma once

#include "forEachAligned.hpp"

#include <cstdint>
#include <random>
#include <cerrno>
#include <cstdint>
#include <system_error>

#include <linux/random.h>
#include <linux/version.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace ccutils {

/** Fill the range `[begin, end)` with random data from a random device. **/
void randomFill(char* begin, char* end);

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 17, 0)

static long getRandom(void* buf, std::size_t buf_len, unsigned int flags) {
    return syscall(SYS_getrandom, buf, buf_len, flags);
}

void randomFill(char* begin, char* end) {
    while (begin < end) {
        long bytes_read = getRandom(begin, end - begin, 0);
        if (bytes_read > 0) {
            begin += bytes_read;
            continue;
        } else if (errno == EINTR) {
            continue;
        } else {
            throw std::system_error(errno, std::system_category());
        }
    }
}

#else  // older versions of Linux

void randomFill(char* begin, char* end) {
    std::random_device rng;
    std::uniform_int_distribution<std::uint32_t> dist;

    forEachAligned<std::uint32_t, std::uint16_t, std::uint8_t>(begin, end, [&](std::uint32_t* p) { *p = dist(rng); },
                                                               [&](std::uint16_t* p) { *p = dist(rng); },
                                                               [&](std::uint8_t* p) { *p = dist(rng); });
}

#endif

/** A seed sequence which pulls data from a random device via \c randomFill. This is a partial implementation of the
 *  seed sequence concept and can be used as one in most cases, but it lacks functions that allow for storing or loading
 *  repeatable state.
 *
 *  \see randomSeeded
 **/
class RandomSeedSeq {
public:
    using ResultType = std::uint32_t;

public:
    void generate(ResultType* begin, ResultType* end) {
        randomFill(reinterpret_cast<char*>(begin), reinterpret_cast<char*>(end));
    }
};

/** Creates a random number engine seeded from a secure seed sequence (\c RandomSeedSeq).
 *
 *  \tparam TRandomNumberEngine The type of seeded random number engine to create (for example, \c std::mt19937,
 *   \c std::ranlux48, or \c std::minstd_rand).
 *
 *  \example
 *  \code
 *  auto rng = randomSeeded<std::mt19937>();
 *  \endcode
 **/
template <typename TRandomNumberEngine>
TRandomNumberEngine randomSeeded() {
    RandomSeedSeq rss;
    return TRandomNumberEngine(rss);
}

auto random() {
    return randomSeeded<std::mt19937>();
}

}  // namespace ccutils
