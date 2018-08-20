#pragma once

#include <utility>

#ifndef ALWAYS_INLINE
#define ALWAYS_INLINE __attribute__((always_inline))
#endif

#ifndef SCOPE_EXIT
namespace ccutils {

template <class F> class scope_guard {
    const F function;

public:
    constexpr scope_guard(const F& function)
        : function{ function } {}
    constexpr scope_guard(F&& function)
        : function{ std::move(function) } {}
    ~scope_guard() { function(); }
};

template <class F> inline scope_guard<F> make_scope_guard(F&& function) {
    return std::forward<F>(function);
}

}

#define SCOPE_EXIT_CONCAT(n, ...)                                                                  \
    const auto scope_exit##n = ccutils::make_scope_guard([&] { __VA_ARGS__; })
#define SCOPE_EXIT_FWD(n, ...) SCOPE_EXIT_CONCAT(n, __VA_ARGS__)
#define SCOPE_EXIT(...) SCOPE_EXIT_FWD(__LINE__, __VA_ARGS__)
#endif

#ifndef MAKE_HASHABLE
// http://stackoverflow.com/a/38140932
//
//  struct SomeHashKey {
//    std::string key1;
//    std::string key2;
//    bool key3;
//  };
//  MAKE_HASHABLE(SomeHashKey, t.key1, t.key2, t.key3)

namespace ccutils {
inline void hash_combine(std::size_t& seed) {}

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    hash_combine(seed, rest...);
}
}

#define MAKE_HASHABLE(type, ...)                                                                   \
    namespace std {                                                                                \
        template <> struct hash<type> {                                                            \
            std::size_t operator()(const type& t) const {                                          \
                std::size_t ret = 0;                                                               \
                ccutils::hash_combine(ret, __VA_ARGS__);                                           \
                return ret;                                                                        \
            }                                                                                      \
        };                                                                                         \
    }
#endif
