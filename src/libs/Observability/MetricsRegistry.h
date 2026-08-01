#ifndef OBSERVABILITY_METRICSREGISTRY_H_
#define OBSERVABILITY_METRICSREGISTRY_H_

#include "Observability/IInterfaceMetrics.h"
#include "Observability/NoOpInterfaceMetrics.h"

// System headers
#include <memory>
#include <utility>

namespace Observability
{

namespace detail
{

/**
 * @brief The process-wide swappable IInterfaceMetrics slot.
 *
 * A function-local static inside an inline function is the C++11-safe way
 * to get one process-wide instance from a header (inline variables are a
 * C++17 feature). The ODR guarantees this merges to a single
 * definition/instance across translation units.
 */
inline std::shared_ptr<IInterfaceMetrics> &registrySlot()
{
   static std::shared_ptr<IInterfaceMetrics> instance = NoOpInterfaceMetrics::instance();
   return instance;
}

} // namespace detail

/**
 * @brief Process-wide accessor for the current recording implementation.
 *
 * Defaults to NoOpInterfaceMetrics until setMetrics() (normally called once
 * by MetricsPipeline::initMetrics()) installs the real recorder. This is a
 * deliberate global-accessor design, not per-instance dependency injection
 * -- see DESIGN.md for the tradeoff being made. DDSPublisher/DDSSubscriber
 * call this directly rather than taking a constructor argument.
 */
inline IInterfaceMetrics &metrics()
{
   return *detail::registrySlot();
}

/**
 * @brief Install a new recording implementation, replacing whatever is
 *        currently registered.
 *
 * Called once by MetricsPipeline::initMetrics() at startup to install the
 * real OtelInterfaceMetrics; tests use ScopedMetrics (see
 * ScopedMetrics.h, Phase D) to install a FakeInterfaceMetrics for the
 * duration of a single test and restore the previous value afterward.
 */
inline void setMetrics(std::shared_ptr<IInterfaceMetrics> impl)
{
   detail::registrySlot() = std::move(impl);
}

} // namespace Observability

#endif // OBSERVABILITY_METRICSREGISTRY_H_
