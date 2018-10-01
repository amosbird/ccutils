#pragma once

#include "macros.hpp"

#include <cstdint>
#include <utility>

namespace ccutils {

template <typename... TFArgs, typename... FApply>
void forEachAligned(char* first, char* last, FApply&&... transform);

template <typename... TFArgs, typename... FApply>
void forEachAligned(const char* first, const char* last, FApply&&... transform);

namespace detail {

template <typename... TFArgs> struct forEachAlignedImpl;

template <typename TFArg, typename... TFArgRest> struct forEachAlignedImpl<TFArg, TFArgRest...> {
    static constexpr std::size_t byte_align = sizeof(TFArg);

    template <typename TChar, typename FApply, typename... FApplyRest>
    ALWAYS_INLINE static TChar* step(
        TChar* first, TChar* last, const FApply& apply, const FApplyRest&... apply_rest) {
        if (reinterpret_cast<std::uintptr_t>(first) % byte_align == 0
            && first + byte_align <= last) {
            apply(reinterpret_cast<TFArg*>(first));
            return first + byte_align;
        } else {
            return forEachAlignedImpl<TFArgRest...>::step(first, last, apply_rest...);
        }
    }
};

template <typename T> struct forEachAlignedImpl<T> {
    static_assert(sizeof(T) == 1, "You must provide a single byte type to forEachAligned");

    template <typename TChar, typename FApply>
    ALWAYS_INLINE static TChar* step(TChar* first, TChar*, const FApply& apply) {
        apply(reinterpret_cast<T*>(first));
        return first + 1;
    }
};

template <typename... TFArgs, typename TChar, typename... FApply>
void forEachAligned(TChar* first, TChar* last, FApply&&... transform) {
    for (; first < last; /* inline */) {
        first = forEachAlignedImpl<TFArgs...>::step(first, last, transform...);
    }
}

} // namespace detail

template <typename... TFArgs, typename... FApply>
void forEachAligned(char* first, char* last, FApply&&... transform) {
    detail::forEachAligned<TFArgs...>(first, last, std::forward<FApply>(transform)...);
}

template <typename... TFArgs, typename... FApply>
void forEachAligned(const char* first, const char* last, FApply&&... transform) {
    detail::forEachAligned<const TFArgs...>(first, last, std::forward<FApply>(transform)...);
}

} // namespace ccutils
