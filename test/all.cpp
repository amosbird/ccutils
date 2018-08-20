#include <functional>
#include <future>
#include <iostream>
#include <mutex>

#include <Spinlock.hpp>
#include <Stopwatch.hpp>
#include <macros.hpp>
#include <random.hpp>
#include <microbench.hpp>

using namespace std;

// atomic_int value = 0;
int value = 0;

// int loop(bool inc, int limit) {
//     static ccutils::Spinlock l;
//     // static mutex l;
//     std::cout << "Started " << inc << " " << limit << std::endl;
//     for (int i = 0; i < limit; ++i) {
//         l.lock();
//         if (inc) {
//             ++value;
//         } else {
//             --value;
//         }
//         l.unlock();
//     }
//     return 0;
// }

int loop(bool inc, int limit) {
    std::cout << "Started " << inc << " " << limit << std::endl;
    for (int i = 0; i < limit; ++i) {
        if (inc) {
            ++value;
        } else {
            --value;
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    cout << ccutils::random()() << endl;
    {
        SCOPE_EXIT({ cout << "second" << endl; });
        cout << "first" << endl;
    }
    Stopwatch w;

    {
        auto _ = w.start();
        auto f = async(launch::async, bind(loop, true, 20000000));
        loop(false, 10000000);
        f.wait();
        cout << value << endl;
    }
    cout << w << endl;
    std::cout << ccutils::microbench<std::chrono::microseconds, 1, 1>([&]() {
        std::cout << "string" << std::endl;
    }) << endl;
}
