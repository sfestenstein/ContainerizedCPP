#ifndef OBSERVABILITY_SCOPEDMETRICS_H_
#define OBSERVABILITY_SCOPEDMETRICS_H_

#include "Observability/IInterfaceMetrics.h"
#include "Observability/MetricsRegistry.h"

// System headers
#include <memory>
#include <utility>

namespace Observability
{

/**
 * @brief RAII guard that installs a replacement IInterfaceMetrics into the
 *        global registry for its lifetime, restoring the previous value on
 *        destruction.
 *
 * Since Observability::metrics() is a process-global registry rather than a
 * constructor-injected dependency (see DESIGN.md), tests need a way to
 * install a FakeInterfaceMetrics for exactly the duration of one test and
 * guarantee it's restored afterward, even on an assertion failure/exception
 * -- otherwise one TEST() could leak its fake into the next one in the same
 * binary.
 *
 * @code
 * auto fake = std::make_shared<Observability::FakeInterfaceMetrics>();
 * Observability::ScopedMetrics guard(fake);
 * // ... exercise code that calls Observability::metrics() ...
 * ASSERT_EQ(fake->sent.size(), 1);
 * @endcode
 */
class ScopedMetrics
{
public:
   explicit ScopedMetrics(std::shared_ptr<IInterfaceMetrics> replacement)
      : _previous(detail::registrySlot())
   {
      detail::registrySlot() = std::move(replacement);
   }

   ~ScopedMetrics()
   {
      detail::registrySlot() = std::move(_previous);
   }

   ScopedMetrics(const ScopedMetrics &) = delete;
   ScopedMetrics &operator=(const ScopedMetrics &) = delete;
   ScopedMetrics(ScopedMetrics &&) = delete;
   ScopedMetrics &operator=(ScopedMetrics &&) = delete;

private:
   std::shared_ptr<IInterfaceMetrics> _previous;
};

} // namespace Observability

#endif // OBSERVABILITY_SCOPEDMETRICS_H_
