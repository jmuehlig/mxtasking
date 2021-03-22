#pragma once

#include <atomic>

namespace mx::util {
/**
 * The maybe_atomic<T> makes T atomic in ARM hardware but none-atomic on x86_64.
 */
#if defined(__x86_64__) && (!defined(__has_feature) || !__has_feature(thread_sanitizer))
template <typename T> using maybe_atomic = T;
#else
template <typename T> using maybe_atomic = std::atomic<T>;
#endif
} // namespace mx::util