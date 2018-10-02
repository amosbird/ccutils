#pragma once

#include <functional>
#include <utility>

#ifndef ALWAYS_INLINE
#define ALWAYS_INLINE __attribute__((always_inline))
#endif
#ifndef NO_INLINE
#define NO_INLINE __attribute__((__noinline__))
#endif
#ifndef MAY_ALIAS
#define MAY_ALIAS __attribute__((__may_alias__))
#endif

#ifndef ANONYMOUS_VARIABLE
#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_IMPL(s1, s2)
#define ANONYMOUS_VARIABLE(str) CONCATENATE(str, __COUNTER__)
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
    template <> struct hash<type> {                                                                \
        std::size_t operator()(const type& t) const {                                              \
            std::size_t ret = 0;                                                                   \
            ccutils::hash_combine(ret, __VA_ARGS__);                                               \
            return ret;                                                                            \
        }                                                                                          \
    };                                                                                             \
    }
#endif
