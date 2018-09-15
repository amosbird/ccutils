#pragma once

#include <atomic>
#include <chrono>

template <typename TClock>
class BasicStopwatch {
public:
    using Clock = TClock;
    using TimePoint = typename Clock::time_point;
    using Duration = typename Clock::duration;
    using TickType = typename Duration::rep;

    class ticker {
    public:
        ticker() : owner_(nullptr) {}

        explicit ticker(BasicStopwatch* owner) : owner_(owner), starttime_(Clock::now()) {}

        ticker(ticker&& source) : owner_(source.owner_), starttime_(source.starttime_) { source.owner_ = nullptr; }

        // disable copying
        ticker(const ticker&) = delete;
        ticker& operator=(const ticker&) = delete;

        ~ticker() {
            if (owner_) {
                Duration tick_time = Clock::now() - starttime_;
                owner_->add(tick_time);
            }
        }

    private:
        BasicStopwatch* owner_;
        TimePoint starttime_;
    };

    using ticker_type = ticker;

public:
    BasicStopwatch() : ticks_(0) {}

    void add(const Duration& tm) { ticks_.fetch_add(tm.count(), std::memory_order_relaxed); }

    void reset() { ticks_ = TickType(0); }

    Duration total() const { return Duration(ticks_); }

    ticker start() { return ticker(this); }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const BasicStopwatch& watch) {
        return stream << std::chrono::duration_cast<std::chrono::milliseconds>(watch.total()).count() << "ms";
    }

private:
    std::atomic<TickType> ticks_;
};

using Stopwatch = BasicStopwatch<std::chrono::high_resolution_clock>;

// int main()
// {
//     high_resolution_stopwatch watch;
//     {
//         auto ticker = watch.start();
//         std::this_thread::sleep_for(std::chrono::seconds(1));
//     }
//     std::cout << watch << std::endl;
// }
