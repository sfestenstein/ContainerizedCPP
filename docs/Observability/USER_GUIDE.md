# Observability Module — User Guide

**Status:** Written against the *proposed* design in
[DESIGN.md](DESIGN.md)/[IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md), so
reviewers can sanity-check the day-to-day ergonomics of the API, not just
the architecture. Nothing described here is built yet — treat code samples
as the intended shape, not a working reference.

## What this module gives you

`Observability` instruments interface traffic (DDS today; the
`IInterfaceMetrics` abstraction is transport-agnostic) and process resource
usage, and exports both as OpenTelemetry metrics, alongside forwarding
application logs as OTel log records. It does **not** (yet) provide
distributed tracing — see [DESIGN.md's "What's not here yet"](DESIGN.md#whats-not-here-yet).

## Configuring the pipeline

At process startup (the *composition root* — see `Radar.cpp`/`Workstation.cpp`),
build the metrics pipeline once:

```cpp
#include "Observability/MetricsPipeline.h"
#include "Observability/ProcessMetrics.h"

auto meterProvider = Observability::initMetrics({
   .serviceName = "RadarDDSRadar",
   .aggregationPeriod = std::chrono::milliseconds(5000),
   .protocol = Observability::ExporterProtocol::Grpc,  // or ::Http, ::Console
});

Observability::ProcessMetrics processMetrics(meterProvider->GetMeter("Observability")); // keep alive for process lifetime
```

`initMetrics()` builds the real `OtelInterfaceMetrics` recorder internally
and registers it as the last thing it does (see the next section) — there's
no separate object to construct or thread through your code.

Logging is unchanged in this pass — `OtelLogSink`'s existing
`Observability::initLogging()` / `Observability::createOtelLogSink()` calls
keep working exactly as they do today, side by side with the metrics setup
above:

```cpp
#include "Observability/OtelLogSink.h"

auto loggerProvider = Observability::initLogging({.serviceName = "RadarDDSRadar"});
CommonUtils::GeneralLogger::addSink(Observability::createOtelLogSink());
```

At shutdown, call `ForceFlush()` on both providers before returning from
`main()` (not `Shutdown()` — see the code comment in `Radar.cpp` about
static-destruction-order hazards with gRPC's global state):

```cpp
meterProvider->ForceFlush();
loggerProvider->ForceFlush();
```

### How metrics get to `DDSPublisher`/`DDSSubscriber`

`DDSPublisher<T>`/`DDSSubscriber<T>` don't take a metrics constructor
argument. Instead, they call `Observability::metrics()` directly — a
process-global registry (defined in `MetricsRegistry.h`) that starts
out pointing at a harmless `NoOpInterfaceMetrics` and gets swapped to the
real `OtelInterfaceMetrics` as the last step of `initMetrics()`. This is a
deliberate tradeoff: a global accessor instead of per-instance dependency
injection, chosen so that no publisher/subscriber constructor or call site
needs to change when the recorder changes. See DESIGN.md for the reasoning.
Call `Observability::setMetrics(...)` yourself only if you need to install a
different recorder — normally `initMetrics()` handles this for you.

### Choosing an exporter protocol

`ExporterProtocol` selects how metrics leave the process (`MetricsOptions`
only in this pass — logging is unchanged and still hardcoded to OTLP/gRPC):

| Value | Transport | When to use |
|---|---|---|
| `Grpc` (default) | OTLP over gRPC | Talking to the otel-collector on its default gRPC port (4317). |
| `Http` | OTLP over HTTP | Same collector, different port/transport — useful to demonstrate both paths work, or in environments where gRPC is blocked. |
| `Console` | stdout | Local debugging with no collector running at all — see the raw metric values printed directly. |

The endpoint itself (for `Grpc`/`Http`) is controlled by OTel's own
environment-variable convention, `OTEL_EXPORTER_OTLP_ENDPOINT` — not a field
on `MetricsOptions`.

## What gets recorded

| Metric | Instrument | Unit | Attributes | Notes |
|---|---|---|---|---|
| `interface.messages.sent` | Counter\<uint64_t\> | `{message}` | `interface.name`, `interface.type`, `topic` | Incremented once per `DDSPublisher<T>::publish()` call. |
| `interface.messages.received` | Counter\<uint64_t\> | `{message}` | same | Incremented once per valid sample in `DDSSubscriber<T>::waitLoop()`. |
| `interface.bytes.sent` | Counter\<uint64_t\> | `By` | same | Currently `sizeof(T)` — the in-memory struct size, not the CDR wire size; wrong for variable-length IDL types (strings/sequences). Good enough to prove the pipeline end-to-end. |
| `interface.bytes.received` | Counter\<uint64_t\> | `By` | same | Same caveat as above. |
| `interface.latency` | Histogram\<double\> | `ms` | same | Send→receive latency, from the message header's `timestamp_ns` to receipt. Only recorded for message types with a `header().timestamp_ns()` field (compile-time trait check — see DESIGN.md). Assumes synchronized clocks between processes; fine for a same-container demo. |
| `interface.subscriber.active` | ObservableGauge\<int64_t\> | `{subscriber}` | same | 1 while a `DDSSubscriber<T>` is running, sampled once per collection cycle. |
| `process.cpu.utilization` | ObservableGauge\<double\> | `1` (ratio) | none | Fraction of one CPU core used since the previous sample, `/proc/self/stat`. |
| `process.memory.usage` | ObservableGauge\<int64_t\> | `By` | none | Current resident set size (RSS), `/proc/self/status` `VmRSS`. |

Attribute values: `interface.name` is the human-readable name passed to a
publisher/subscriber's constructor (e.g. `"RadarTrackPub"`), `interface.type`
is one of the `Observability::InterfaceType::*` constants (currently only
`DDS_INTERFACE` has a real transport behind it), `topic` is the DDS topic
name.

## Resource attributes

Every metric record carries these resource attributes, set once by
`MetricsPipeline`'s internal resource-building step:

| Attribute | Source |
|---|---|
| `service.name` | `MetricsOptions::serviceName` (required) |
| `service.version` | CMake-injected `-D` compile definition off the project version |
| `service.instance.id` | hostname + PID, computed once at startup |
| `deployment.environment` | `getenv("DEPLOYMENT_ENVIRONMENT")`, defaults to `"development"` |

Set `DEPLOYMENT_ENVIRONMENT` before launching an app if you want metrics
tagged for a specific environment (e.g. `staging`, `production`).

## Viewing the data

For local iteration, construct the pipeline with
`.protocol = ExporterProtocol::Console` and read values directly off
stdout — no collector required. This is useful when tuning the
`interface.latency` histogram's View boundaries against real observed
values before committing to them.

For a running deployment, both apps push OTLP (gRPC or HTTP, per
`ExporterProtocol`) to the otel-collector. Where the collector forwards
data from there is a separate, not-yet-decided effort — see
`deploy/observability/` for the current (pre-redesign) collector
configuration, which is out of scope for this document.

## Testing against `IInterfaceMetrics`

`DDSPublisher<T>`/`DDSSubscriber<T>` call `Observability::metrics()`
directly rather than taking a constructor argument (see "How metrics get to
`DDSPublisher`/`DDSSubscriber`" above), so tests install a test double into
the registry for the duration of the test using `ScopedMetrics`:

- **Default (`NoOpInterfaceMetrics`)** — tests that don't install a
  `ScopedMetrics` guard get the harmless no-op automatically; nothing to do.
- **`Observability::FakeInterfaceMetrics`** — an in-memory recorder for
  tests that *do* care. Construct one, wrap it in a `ScopedMetrics` guard,
  and assert against its `sent`/`received`/`latencies` vectors once the
  guard is in scope:

  ```cpp
  auto fake = std::make_shared<Observability::FakeInterfaceMetrics>();
  Observability::ScopedMetrics guard(fake);
  CycloneDDS::DDSPublisher<MyMessage> pub(domainId, entry, "MyPub"); // no metrics arg
  pub.publish(msg);
  ASSERT_EQ(fake->sent.size(), 1);
  EXPECT_EQ(fake->sent[0].interfaceName, "MyPub");
  ```

  `ScopedMetrics`'s destructor restores whatever was registered before the
  guard was constructed, even if an assertion throws — so one `TEST()`
  can't leak its fake into the next one in the same binary.

No mocking framework is required for this — see `IMPLEMENTATION_PLAN.md`
Phase D for the full test plan.

## What's deliberately not here yet

This module does not currently provide distributed tracing/spans,
trace-context propagation across DDS process boundaries, baggage
propagation, exemplars, or log-trace correlation. See
[DESIGN.md — What's not here yet](DESIGN.md#whats-not-here-yet) for what
each of these would take to add.
