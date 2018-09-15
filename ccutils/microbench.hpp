#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <vector>

namespace ccutils {

// this class is based on https://github.com/cameron314/microbench
class Stats {
public:
    Stats(std::vector<double>& results) {
        auto n = results.size();
        std::sort(results.begin(), results.end());

        _min = results[0];
        _max = results.back();

        if (n == 1) {
            _q[0] = _q[1] = _q[2] = results[0];
            _avg = results[0];
            _variance = 0;
            return;
        }

        double sum = 0;
        double c = 0;
        for (auto r : results) {
            auto y = r - c;
            auto t = sum + y;
            c = (t - sum) - y;
            sum = t;
        }
        _avg = sum / n;

        sum = 0, c = 0;
        for (auto r : results) {
            auto y = (r - _avg) * (r - _avg) - c;
            auto t = sum + y;
            c = (t - sum) - y;
            sum = t;
        }
        _variance = sum / (n - 1);

        _q[1] = (n & 1) == 0 ? (results[n / 2 - 1] + results[n / 2]) * 0.5 : results[n / 2];
        if ((n & 1) == 0) {
            _q[0] = (n & 3) == 0 ? (results[n / 4 - 1] + results[n / 4]) * 0.5 : results[n / 4];
            _q[2] = (n & 3) == 0 ? (results[n / 2 + n / 4 - 1] + results[n / 2 + n / 4]) * 0.5
                                 : results[n / 2 + n / 4];
        } else if ((n & 3) == 1) {
            _q[0] = results[n / 4 - 1] * 0.25 + results[n / 4] * 0.75;
            _q[2] = results[n / 4 * 3] * 0.75 + results[n / 4 * 3 + 1] * 0.25;
        } else {
            _q[0] = results[n / 4] * 0.75 + results[n / 4 + 1] * 0.25;
            _q[2] = results[n / 4 * 3 + 1] * 0.25 + results[n / 4 * 3 + 2] * 0.75;
        }
    }

    inline double min() const { return _min; }
    inline double max() const { return _max; }
    inline double range() const { return _max - _min; }
    inline double avg() const { return _avg; }
    inline double variance() const { return _variance; }
    inline double stddev() const { return std::sqrt(_variance); }
    inline double median() const { return _q[1]; }
    inline double q1() const { return _q[0]; }
    inline double q2() const { return _q[1]; }
    inline double q3() const { return _q[2]; }

private:
    double _min;
    double _max;
    double _q[3];
    double _avg;
    double _variance;
};

template <typename Resolution = std::chrono::nanoseconds, std::size_t iter = 1,
    std::size_t run = 100, bool timePerIter = true, typename TFunc>
Stats microbenchStats(TFunc&& func) {
    static_assert(run >= 1);
    static_assert(iter >= 1);

    std::vector<double> results(run);
    for (std::size_t i = 0; i < run; ++i) {
        auto start = std::chrono::steady_clock::now();
        std::atomic_signal_fence(std::memory_order_acq_rel);
        for (std::size_t j = 0; j < iter; ++j) {
            func();
        }
        std::atomic_signal_fence(std::memory_order_acq_rel);
        auto t = std::chrono::steady_clock::now();
        results[i] = std::chrono::duration_cast<Resolution>(t - start).count();
        if (timePerIter) {
            results[i] /= iter;
        }
    }

    double fastest = results[0];
    for (std::size_t i = 1; i < run; ++i) {
        if (results[i] < fastest) {
            fastest = results[i];
        }
    }

    Stats stats(results);
    return stats;
}

template <typename Resolution = std::chrono::nanoseconds, std::size_t iter = 1,
    std::size_t run = 100, bool timePerIter = true, typename TFunc>
inline __attribute__((always_inline)) double microbench(TFunc&& func) {
    return microbenchStats<Resolution, iter, run, timePerIter, TFunc>(std::forward<TFunc>(func)).avg();
}

}
