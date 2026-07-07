#ifndef CROWCOMPAT_H_
#define CROWCOMPAT_H_

/**
 * @file CrowCompat.h
 * @brief Compatibility shim for Crow on C++20 / newer libc++.
 *
 * Crow internally uses  ostringstream << std::atomic<unsigned>  which is
 * ambiguous when libc++ exposes explicit integer conversion operators.
 * Including this header before <crow.h> resolves the ambiguity.
 */

#include <atomic>
#include <ostream>
#include <type_traits>

template <typename T>
auto operator<<(std::ostream &os, const std::atomic<T> &a)
   -> std::enable_if_t<std::is_integral_v<T>, std::ostream &>
{
   return os << a.load();
}

#include <crow.h>

#endif // CROWCOMPAT_H_
