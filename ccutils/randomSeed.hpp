#pragma once

#include <SipHash.hpp>

#include <time.h>
#include <unistd.h>
#include <sys/types.h>

uint64_t randomSeed()
{
    struct timespec times;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &times))
        return 0;
    SipHash hash;
    hash.update(times.tv_nsec);
    hash.update(times.tv_sec);
    hash.update(getpid());
    hash.update(&times);
    return hash.get64();
}
