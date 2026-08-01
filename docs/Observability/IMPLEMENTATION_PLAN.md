# Observability Module — Implementation Plan

**Status:** Proposed — not yet implemented. This is the phased breakdown for
the redesign described in [DESIGN.md](DESIGN.md), presented for engineering
review before any code changes start. Once the team agrees on the approach
and implementation proceeds, this doc should get a short follow-up pass
noting any deltas between what's proposed here and what was actually built.

See [DESIGN.md](DESIGN.md) for the *why* (SOLID smells being fixed, class
diagram, sequence diagrams). See [USER_GUIDE.md](USER_GUIDE.md) for how the
module is used once built. This document is the *how* and *in what order*.

## Phase A — Break the singleton's rigidity; split responsibilities (no behavior change)

Goal: fix the "no test seam" and SRP violations with zero change in runtime
behavior, so this phase is safe to review and land independently of
everything else. `Observability::metrics()` stays a global accessor by
design (see DESIGN.md) — the fix is making what it returns swappable, not
replacing it with constructor injection. `OtelLogSink`/logging is
deliberately **not** touched in this phase; it keeps working exactly as it
does today.

**New files in `src/libs/Observability/`, all header-only except where noted:**

- `IInterfaceMetrics.h` — the pure interface only, no OTel includes, no
  registry:
  ```cpp
  namespace Observability {
  namespace InterfaceType { constexpr const char *DDS_INTERFACE = "DDS"; /* GRPC_INTERFACE, ZMQ_INTERFACE unchanged */ }
  class IInterfaceMetrics {
  public:
     virtual ~IInterfaceMetrics() = default;
     virtual void recordSent(const std::string &interfaceName, const char *type,
                              const std::string &topic, uint64_t bytes) = 0;
     virtual void recordReceived(const std::string &interfaceName, const char *type,
                                  const std::string &topic, uint64_t bytes) = 0;
     // recordLatency / registerActiveProbe / unregisterActiveProbe added in Phase C
  };
  }
  ```
  Fixes a layering smell where `DDSPublisher.h` currently pulls in
  `opentelemetry/sdk/metrics/meter_provider.h` transitively just to see the
  interface.
- `NoOpInterfaceMetrics.h` — **header-only**, includes `IInterfaceMetrics.h`.
  A stateless Null Object implementing `IInterfaceMetrics` with empty
  bodies, plus `static std::shared_ptr<IInterfaceMetrics> instance();`:
  ```cpp
  namespace Observability {
  class NoOpInterfaceMetrics : public IInterfaceMetrics {
  public:
     void recordSent(const std::string &, const char *, const std::string &, uint64_t) override {}
     void recordReceived(const std::string &, const char *, const std::string &, uint64_t) override {}
     static std::shared_ptr<IInterfaceMetrics> instance() {
        static auto shared = std::make_shared<NoOpInterfaceMetrics>();
        return shared;
     }
  };
  }
  ```
- `MetricsRegistry.h` — **header-only**, includes `NoOpInterfaceMetrics.h`
  (needed for its default value — this is why the registry can't live in
  `IInterfaceMetrics.h`: that would require `IInterfaceMetrics.h` to depend
  on a class that implements it, a circular include). Holds the swappable
  global pointer:
  ```cpp
  namespace Observability {
  namespace detail {
  // Function-local static inside an inline function: the C++11-safe way to get
  // one process-wide instance from a header (no inline variables until C++17).
  // The ODR guarantees this merges to a single definition/instance across TUs.
  inline std::shared_ptr<IInterfaceMetrics> &registrySlot() {
     static std::shared_ptr<IInterfaceMetrics> instance = NoOpInterfaceMetrics::instance();
     return instance;
  }
  }

  inline IInterfaceMetrics &metrics() { return *detail::registrySlot(); }
  inline void setMetrics(std::shared_ptr<IInterfaceMetrics> impl) {
     detail::registrySlot() = std::move(impl);
  }
  }
  ```
  Any file that calls `Observability::metrics()` (i.e. `DDSPublisher.h`/
  `DDSSubscriber.h`) includes `MetricsRegistry.h` — still no OTel includes
  anywhere in this chain, so the original layering goal (no transitive OTel
  SDK headers) holds.
- `OtelInterfaceMetrics.h`/`.cpp` — the concrete recorder, constructed with
  an explicit `opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>`
  passed in (not looked up via the global `Provider` on every call). All 4
  counters are created **once in the constructor**, replacing today's
  function-local-static lazy lookups.
- `MetricsPipeline.h`/`.cpp` — pipeline-building only (moved out of
  `InterfaceMetrics.cpp`): builds the resource (a small private helper in
  the `.cpp`, not a separate class — it now has exactly one caller), builds
  the exporter, reader, context, registers the provider globally via
  `SetMeterProvider` (the one legitimate global registration OTel's design
  requires), constructs `OtelInterfaceMetrics` and calls
  `Observability::setMetrics(...)` with it as the last step, then returns
  the SDK provider. `MetricsOptions` (`serviceName`, `aggregationPeriod`)
  moves here.

**Modified files:**

- `InterfaceMetrics.h`/`.cpp` — **deleted**, replaced entirely by the files
  above.
- `src/libs/Observability/CMakeLists.txt` — source list becomes
  `MetricsPipeline.cpp OtelInterfaceMetrics.cpp OtelLogSink.cpp` (`NoOp` and
  the interface/registry are header-only; `OtelLogSink.cpp` is unmodified
  but stays in the list since it isn't going anywhere).
- `src/apps/RadarDDSDemo/Radar.cpp` / `Workstation.cpp` — composition root:
  ```cpp
  auto meterProvider = Observability::initMetrics({.serviceName = "RadarDDSRadar"});
  // OtelLogSink is unchanged in this pass — existing initLogging()/createOtelLogSink() calls stay as-is.
  ```
  **No other changes to these files.** `DDSPublisher<T>`/`DDSSubscriber<T>`
  constructions are untouched — they never took a metrics argument before
  and still don't.

**Minimal-touch files (one line, no logic change):**

- `src/libs/CycloneDDS/DDSPublisher.h` / `DDSSubscriber.h` — their existing
  `Observability::metrics().recordSent(...)` / `recordReceived(...)` call
  sites are source-compatible as-is: `metrics()` now returns
  `IInterfaceMetrics&` instead of the old concrete type, which is a
  transparent change to these callers. The only edit needed is the
  `#include` line: swap whatever currently pulls in the deleted
  `InterfaceMetrics.h` for `#include "Observability/MetricsRegistry.h"`.
  No constructor-signature edits, no call-site logic changes, and zero
  edits needed in `DDSPublisherUt.cpp`/`DDSSubscriberUt.cpp`.

**Files explicitly NOT modified in this phase:**

- `OtelLogSink.h`/`.cpp` — deferred; still hardcoded OTLP/gRPC, still uses
  its own `Provider::GetLoggerProvider()` lookup per `sink_it_()`, exactly
  as today.

**Design decisions made (already confirmed, not open for re-litigation
without reason):**
- The registry is process-global by design, not a per-instance DI seam —
  chosen specifically to avoid touching `DDSPublisher`/`DDSSubscriber` or
  their ~10 call sites. See DESIGN.md's "Why redesign it?" for the tradeoff
  being accepted (global mutable state) versus what's still fixed (a real
  seam to substitute `NoOp`/`Otel`/`Fake`, where today there is none).
- `InterfaceMetrics.h`/`.cpp` are deleted outright rather than kept as a
  thin aggregator header — one header per responsibility matches the SRP
  story being taught.

**Verification:** should produce **identical runtime behavior** to today
(same exporter, same attributes; default registry state is `NoOp`, matching
today's pre-init behavior). Run the existing `DDSPublisherUt`/
`DDSSubscriberUt` suite unchanged, then run Radar+Workstation and confirm
metrics via `ExporterProtocol::Console` output or the otel-collector's
`debug` exporter logs.

## Phase B — Exporter strategy (OCP)

Goal: make the OTLP transport a config value instead of a compile-time
choice, and demonstrate OCP by making a third exporter (console) trivial to
add.

**New files:**

- `ExporterProtocol.h`: `enum class ExporterProtocol { Grpc, Http, Console };`
- `MetricsExporterFactory.h`/`.cpp`:
  `std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> createMetricExporter(ExporterProtocol protocol);`
  — switches between `OtlpGrpcMetricExporterFactory`,
  `OtlpHttpMetricExporterFactory`, and `OStreamMetricExporterFactory`
  (console exporter ships in core SDK, already built — no Dockerfile change
  needed for it).

**Modified files:**

- `MetricsOptions` gains `ExporterProtocol protocol = ExporterProtocol::Grpc;`.
- `MetricsPipeline.cpp` calls the new factory instead of hardcoding
  `OtlpGrpcMetricExporterFactory::Create`.
- `src/libs/Observability/CMakeLists.txt` — link
  `opentelemetry-cpp::otlp_http_metric_exporter`,
  `opentelemetry-cpp::ostream_metrics_exporter` alongside the existing gRPC
  ones. (The Dockerfile already has `WITH_OTLP_HTTP=ON` +
  `libcurl4-openssl-dev` in flight as an uncommitted change — no further
  Dockerfile edits needed.)
- Flip one of `Radar.cpp`/`Workstation.cpp` to
  `.protocol = ExporterProtocol::Http` so both transports are actually
  exercised at runtime, not just compiled.

**Verification:** confirm the otel-collector still receives data from both
apps (one over gRPC, one over HTTP) — check its `debug` exporter's log
output, no dashboard needed. Logging (`OtelLogSink`) is still untouched and
still hardcoded OTLP/gRPC in this phase.

## Phase C — New interface-traffic signals (no tracing)

Goal: exercise histograms, observable gauges, and Views on top of the
already-tracked interface traffic (send/receive counts).

**Modified `IInterfaceMetrics.h` / `NoOpInterfaceMetrics.h` /
`OtelInterfaceMetrics.h`/`.cpp`:**

- Add `recordLatency(interfaceName, type, topic, std::chrono::nanoseconds latency)`
  — backed by a new `Histogram<double>` (`interface.latency`, unit `ms`).
- Add `registerActiveProbe(interfaceName, type, topic, const std::atomic<bool> *activeFlag)`
  and `unregisterActiveProbe(const std::atomic<bool> *activeFlag)` — backed
  by an `Int64ObservableGauge` (`interface.subscriber.active`). Because
  OTel's `AddCallback` takes a raw function pointer + `void*` state (not
  `std::function`), `OtelInterfaceMetrics` keeps a mutex-guarded
  `unordered_map<const std::atomic<bool>*, ProbeInfo>`; the static callback
  dereferences only the atomic, never `this` of the subscriber — avoids any
  dangling-`this` hazard if a `DDSSubscriber` outlives/predates gauge
  collection.

**New file `src/libs/CycloneDDS/DDSMessageTraits.h`:**

- SFINAE trait `has_timestamp_header_v<T>` detecting
  `.header().timestamp_ns()`, and `extractLatencySinceSend<T>(const T&)`
  returning `std::optional<std::chrono::nanoseconds>`. Compiles out cleanly
  (returns `std::nullopt`) for message types without a header (e.g. the DDS
  test suite's `TestMessage`), so no IDL changes are needed and no
  transport is forced to support latency.

**Modified `DDSSubscriber.h`:**

- Constructor: `Observability::metrics().registerActiveProbe(_interfaceName, InterfaceType::DDS_INTERFACE, _entry.topicName, &_running);`
- Destructor: `Observability::metrics().unregisterActiveProbe(&_running);`
- `waitLoop()`: after `recordReceived`, if `extractLatencySinceSend(sample.data())`
  returns a non-negative duration, call `Observability::metrics().recordLatency(...)`.
  Add a short comment noting `system_clock` isn't synchronized across
  processes/hosts in general — fine for the same-container demo this branch
  targets, would need a monotonic round-trip or NTP/PTP assumption
  otherwise.

**Modified `MetricsPipeline.cpp`:**

- Replace the currently-empty `ViewRegistryFactory::Create()` with one
  registered View for `interface.latency`, using explicit histogram bucket
  boundaries (e.g. `{0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10, 25, 50, 100, 250, 500}`
  ms — see DESIGN.md's "Why Views?" section for the rationale). Consider
  running once with the `Console` exporter from Phase B to sanity-check
  real latency values before finalizing boundaries.
- Extend the private resource-building helper (folded in during Phase A) to
  also add `service.version` (from a CMake-injected `-D` compile
  definition), `service.instance.id` (hostname + pid), and
  `deployment.environment` (from `getenv`, default `"development"`).

**Verification:** check `ExporterProtocol::Console` output or the
otel-collector's `debug` exporter logs for `interface.latency` and
`interface.subscriber.active` alongside the unchanged Phase A/B counters.

## Phase C2 — Process resource metrics (CPU/Memory)

Goal: a second, independent use of Observable Gauges — simpler than the
subscriber-active case since there's exactly one process to sample, so no
probe registry is needed.

**New file `src/libs/Observability/ProcessSampler.h`/`.cpp`** — pure logic,
zero OTel includes, independently unit-testable:

```cpp
namespace Observability {
struct ProcessSample {
   double cpuUtilization;        // fraction of one core used since the previous sample() call, 0..1+
   int64_t residentMemoryBytes;  // current RSS, from /proc/self/status VmRSS
};
class ProcessSampler {
public:
   ProcessSample sample(); // reads /proc/self/stat (utime+stime) + /proc/self/status (VmRSS),
                            // computes cpuUtilization as a delta against the previous call
private:
   long _clockTicksPerSec = sysconf(_SC_CLK_TCK);
   uint64_t _previousCpuTicks = 0;
   std::chrono::steady_clock::time_point _previousSampleTime;
   bool _hasPreviousSample = false;
};
}
```

First call has no prior delta to compare against, so it returns
`cpuUtilization = 0.0` for that call only — document this in a comment.

**New file `src/libs/Observability/ProcessMetrics.h`/`.cpp`** — the OTel
wiring, same shape as `OtelInterfaceMetrics`:

```cpp
namespace Observability {
class ProcessMetrics {
public:
   explicit ProcessMetrics(opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter);
   ~ProcessMetrics(); // RemoveCallback for both gauges
private:
   static void observeCpu(opentelemetry::metrics::ObserverResult result, void *state);
   static void observeMemory(opentelemetry::metrics::ObserverResult result, void *state);
   ProcessSampler _sampler;
   opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> _cpuGauge;
   opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> _memoryGauge;
};
}
```

Instrument names follow OTel semantic conventions:
`process.cpu.utilization` (`CreateDoubleObservableGauge`, unit `1`, ratio
0..1) and `process.memory.usage` (`CreateInt64ObservableGauge`, unit `By`).
Both callbacks call `_sampler.sample()` directly — no shared
state/registry needed since there's only one process, unlike
`interface.subscriber.active`'s multi-instance case in Phase C.

**Modified `Radar.cpp`/`Workstation.cpp`:** construct once, alongside the
other providers, and keep alive for the process lifetime:

```cpp
Observability::ProcessMetrics processMetrics(meterProvider->GetMeter("Observability"));
```

**Modified `src/libs/Observability/CMakeLists.txt`:** add
`ProcessSampler.cpp ProcessMetrics.cpp` to the source list. No new link
dependencies — `/proc` parsing and `sysconf`/`getrusage` are POSIX/glibc,
already available (this component is Linux-only, consistent with the rest
of this container-based project).

**Verification:** confirm `process.cpu.utilization` moves when triggering
`--stress` mode on Radar, and `process.memory.usage` stays roughly
flat/grows slowly as expected for this workload.

## Phase D — Tests

Goal: real assertions on recorded metrics, not just "doesn't throw."

**New file `src/libs/Observability/FakeInterfaceMetrics.h`** (header-only,
plain in-memory fake, not gmock — matches the "textbook SOLID" simplicity
goal even though `GTest::gmock` is already linked into `DDSTests`):

```cpp
namespace Observability {
struct RecordedCall { std::string interfaceName; std::string type; std::string topic; uint64_t bytes; };
struct RecordedLatency { std::string interfaceName; std::string type; std::string topic; std::chrono::nanoseconds latency; };
class FakeInterfaceMetrics : public IInterfaceMetrics {
public:
   void recordSent(const std::string &interfaceName, const char *type,
                    const std::string &topic, uint64_t bytes) override;
   void recordReceived(...) override;
   void recordLatency(...) override;
   void registerActiveProbe(...) override;
   void unregisterActiveProbe(...) override;

   std::mutex mutex;
   std::vector<RecordedCall> sent, received;
   std::vector<RecordedLatency> latencies;
};
}
```

**New file `src/libs/Observability/ScopedMetrics.h`** (header-only, includes
`MetricsRegistry.h` for access to `detail::registrySlot()`) — since
`Observability::metrics()` is a process-global registry rather than a
constructor-injected dependency (see Phase A / DESIGN.md), tests need a way
to install a `FakeInterfaceMetrics` for exactly the duration of one test and
guarantee it's restored afterward, even on an assertion failure:

```cpp
namespace Observability {
class ScopedMetrics {
public:
   explicit ScopedMetrics(std::shared_ptr<IInterfaceMetrics> replacement)
       : _previous(detail::registrySlot()) {
      detail::registrySlot() = std::move(replacement);
   }
   ~ScopedMetrics() { detail::registrySlot() = std::move(_previous); }
   ScopedMetrics(const ScopedMetrics &) = delete;
   ScopedMetrics &operator=(const ScopedMetrics &) = delete;
private:
   std::shared_ptr<IInterfaceMetrics> _previous;
};
}
```

**Modified `tests/DDSTests/DDSPublisherUt.cpp` / `DDSSubscriberUt.cpp`:**

- Add real assertions using a `FakeInterfaceMetrics` installed via a local
  `ScopedMetrics` guard — e.g.:
  ```cpp
  auto fake = std::make_shared<Observability::FakeInterfaceMetrics>();
  Observability::ScopedMetrics guard(fake);
  CycloneDDS::DDSPublisher<MyMessage> pub(domainId, entry, "MyPub"); // no metrics arg
  pub.publish(msg);
  ASSERT_EQ(fake->sent.size(), 1);
  EXPECT_EQ(fake->sent[0].interfaceName, "MyPub");
  ```
  Receive a message, assert `received.size() >= 1`. Add a case using the DDS
  test suite's `TestMessage` (no header) proving `latencies` stays empty —
  a direct, executable proof that the trait-based extraction degrades
  safely.
- The existing ~10 metrics-indifferent test constructions are unaffected —
  they never touched metrics before and still don't; the registry defaults
  to `NoOpInterfaceMetrics` for any test that doesn't install a
  `ScopedMetrics` guard.

**New `tests/ObservabilityTests/` directory** (added to root
`CMakeLists.txt`'s test subdirectory list):

- Unit tests for `ProcessSampler` — asserting `residentMemoryBytes > 0` and
  that `cpuUtilization` stays within a sane range (e.g. `[0, coreCount]`)
  across two `sample()` calls with a short sleep between them. No
  fake/mock needed, no OTel SDK involved, since `ProcessSampler` has zero
  OTel dependency.

**Modified `tests/DDSTests/CMakeLists.txt`** — link `Observability`
explicitly (likely already transitive via `CycloneDDSLib`, but this
documents the direct dependency now that tests use
`Observability::FakeInterfaceMetrics` directly).

## Suggested order

1. **Phase A** — highest value, lowest risk, touches only an `#include` line
   outside `Observability` itself (no call-site or constructor-signature
   changes); land and verify first.
2. **Phase B** — small and mechanical.
3. **Phase C** — resource attributes first (trivial), then the latency
   histogram + View (self-contained), then the observable gauge (has the
   lifetime/callback-signature subtlety — worth an early compile check
   since async instruments are the least-travelled part of the SDK here).
4. **Phase C2** — independent of Phase C's interface-metrics work; can be
   done in parallel with it, or right after since it reuses the same
   "callback + ObservableGauge" pattern once that's been proven out.
5. **Phase D** — the `ScopedMetrics`/`FakeInterfaceMetrics` tests can start
   as soon as Phase A lands (the registry exists from the start of Phase A);
   `ProcessSampler` tests can start as soon as Phase C2 lands, since it
   needs no OTel SDK/fake at all; latency/gauge-specific test cases wait on
   Phase C.

## Full verification checklist (once implementation proceeds)

- Build the project (`cmake --build`) after each phase; Phase A should not
  change observed metric values or attributes at all — verify via
  `ExporterProtocol::Console` output or the otel-collector's `debug`
  exporter logs.
- Run `RadarDDSDemo` (Radar + Workstation) and check
  `interface.messages.sent/received`, `interface.bytes.sent/received`
  (Phase A/B unchanged), then `interface.latency` and
  `interface.subscriber.active` (Phase C, new), then
  `process.cpu.utilization` and `process.memory.usage` (Phase C2, new).
- After Phase B, confirm the otel-collector receives data from both the
  gRPC-configured and HTTP-configured app.
- Run the existing test suite (`ctest` or the project's test target) after
  Phase A to confirm `DDSPublisherUt`/`DDSSubscriberUt` still pass, after
  Phase C2 to confirm the new `ProcessSampler` unit tests pass, then again
  after Phase D to confirm the new metrics-assertion tests pass.
- No UI is involved; this is backend/library work — verification is via
  build, tests, and observing metrics via the console exporter or the
  otel-collector's own logs. The eventual downstream export target
  (OpenSearch or a direct HTTP frontend) is a separate future effort, not
  covered by this verification.
