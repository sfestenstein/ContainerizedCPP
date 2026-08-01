# Observability Module — Design Document

**Status:** Proposed — under review. Nothing in this document has been
implemented yet; it describes the target design for a SOLID / OpenTelemetry
redesign of the `Observability` module, for discussion before any code
changes land.

## Why redesign it?

The current `Observability` module (`src/libs/Observability/InterfaceMetrics.h`/
`.cpp`, `OtelLogSink.h`/`.cpp`) works and is simple, but demonstrates several
classic OO smells worth fixing:

- **No test seam** — `Observability::metrics()` is a Meyers singleton that
  hardcodes construction of `OtelInterfaceMetrics` internally. It's a
  Service Locator wearing an interface (`IInterfaceMetrics`), but the
  concrete type behind it can never actually be swapped: there's no way to
  substitute a test double or a no-op default. `DDSPublisher`/
  `DDSSubscriber` calling a global accessor is a deliberate, kept design
  choice in this redesign (matches existing call sites, no constructor
  churn) — the fix is making *what the global returns* swappable via a
  small registry (`setMetrics()`), not replacing the global accessor with
  constructor injection. This is a conscious global-state tradeoff, not an
  oversight: it trades per-instance substitutability for zero call-site
  changes.
- **SRP violation** — `InterfaceMetrics.cpp` and `OtelLogSink.cpp` each mix
  three responsibilities in one file: OTel SDK pipeline construction
  (exporter, reader/processor, resource, provider registration), the actual
  recording/business logic, and the global provider registration side
  effect. The two files also duplicate ~25 lines of near-identical
  scaffolding between the metrics and logs paths.
- **OCP violation** — the exporter transport (OTLP/gRPC) is hard-coded, even
  though OTLP/HTTP is already built by the project's Dockerfile and
  currently unused. Swapping transport requires editing and recompiling the
  library rather than changing a config value.
- **Underused OTel surface** — only 4 counters exist today. OpenTelemetry
  offers histograms, observable (pull-based) gauges, Views, and richer
  resource semantic conventions, none of which are used.
- **No tests** — nothing exercises `Observability` directly, and the
  existing DDS unit tests can't assert anything about metrics because
  there's no seam to substitute a test double.

## Goals

- Fix the issues above with a small number of well-named,
  well-motivated abstractions — a textbook SOLID example, not a deep
  enterprise hierarchy.
- Expand the emitted signal set: a send→receive latency histogram, an
  observable gauge for subscriber activity, and observable gauges for
  process CPU/memory usage — while giving the reader hands-on exposure to
  more of the OTel API surface (histograms, Views, observable/pull
  instruments, resource semantic conventions, exporter selection).
- Make the OTLP transport (gRPC vs HTTP) a configuration choice instead of a
  compile-time one.

**Explicitly out of scope for this pass:** distributed tracing/spans,
trace-context propagation through DDS message headers, baggage propagation,
exemplars, and log-trace correlation. These are natural next steps once the
team wants to go further — see
[What's Not Here Yet](#whats-not-here-yet).

## Push vs. pull instruments

OpenTelemetry has two fundamentally different ways an instrument gets its
value, and this design deliberately uses both:

- **Synchronous (push)** — `Counter::Add()`, `Histogram::Record()`. Code
  calls these inline, at the moment an event happens. `recordSent`,
  `recordReceived`, and `recordLatency` all work this way: a message is
  published or received, so the call site immediately pushes a value.
- **Asynchronous / Observable (pull)** — `ObservableGauge`. A callback is
  registered once, and the OTel SDK calls it on its own schedule (every
  `aggregationPeriod`), asking "what's the value right now?" There's no
  event to hook for "a subscriber is idle" or "how much memory is the
  process using right now" — nothing *happens* — so the SDK has to come and
  ask. `interface.subscriber.active`, `process.cpu.utilization`, and
  `process.memory.usage` are all observable gauges for this reason.

A **probe**, in this design, is not an OTel term — it's the app-level
registration record (`registerActiveProbe`/`unregisterActiveProbe`) that
lets multiple `DDSSubscriber` *instances* report into one shared
`interface.subscriber.active` gauge instrument. OTel's `ObservableGauge` is
normally a singleton per `Meter`, with one callback expected to report
values for every attribute-combination that exists — but `DDSSubscriber`
instances come and go at runtime and the gauge doesn't know about them on
its own. Each subscriber hands over a pointer to its own `atomic<bool>
_running` flag in its constructor; `OtelInterfaceMetrics` keeps these in a
map and walks it once per collection cycle. Process CPU/memory sampling
needs no such registry — there's exactly one process, so one callback
samples and reports directly.

## Class diagram

```mermaid
classDiagram
    class IInterfaceMetrics {
        <<interface>>
        +recordSent(interfaceName, type, topic, bytes)
        +recordReceived(interfaceName, type, topic, bytes)
        +recordLatency(interfaceName, type, topic, latency)
        +registerActiveProbe(interfaceName, type, topic, activeFlag)
        +unregisterActiveProbe(activeFlag)
    }
    class MetricsRegistry {
        <<free functions in MetricsRegistry.h>>
        +metrics() IInterfaceMetrics&
        +setMetrics(shared_ptr~IInterfaceMetrics~)
    }
    class OtelInterfaceMetrics {
        -Counter~uint64_t~ messagesSent
        -Counter~uint64_t~ messagesReceived
        -Counter~uint64_t~ bytesSent
        -Counter~uint64_t~ bytesReceived
        -Histogram~double~ latencyMs
        -ObservableInstrument activeGauge
        -map~atomic~bool~*, ProbeInfo~ probes
    }
    class NoOpInterfaceMetrics {
        +instance() shared_ptr~IInterfaceMetrics~
    }
    class FakeInterfaceMetrics {
        +vector~RecordedCall~ sent
        +vector~RecordedCall~ received
        +vector~RecordedLatency~ latencies
    }
    IInterfaceMetrics <|.. OtelInterfaceMetrics
    IInterfaceMetrics <|.. NoOpInterfaceMetrics
    IInterfaceMetrics <|.. FakeInterfaceMetrics
    MetricsRegistry ..> IInterfaceMetrics : holds current shared_ptr~IInterfaceMetrics~ (defaults to NoOp)

    class DDSPublisher~T~ {
        +publish(message)
    }
    class DDSSubscriber~T~ {
        -atomic~bool~ running
        +waitLoop()
    }
    DDSPublisher ..> MetricsRegistry : recordSent via metrics()
    DDSSubscriber ..> MetricsRegistry : recordReceived via metrics()

    class MetricsPipeline {
        +initMetrics(MetricsOptions) shared_ptr~MeterProvider~
    }
    class MetricsExporterFactory {
        +createMetricExporter(ExporterProtocol) unique_ptr~PushMetricExporter~
    }
    class ExporterProtocol {
        <<enumeration>>
        Grpc
        Http
        Console
    }
    MetricsPipeline ..> MetricsExporterFactory : uses
    MetricsExporterFactory ..> ExporterProtocol : selects on
    MetricsPipeline ..> OtelInterfaceMetrics : builds via Meter, then setMetrics(...)
    MetricsPipeline ..> MetricsRegistry : setMetrics(make_shared~OtelInterfaceMetrics~(meter))

    class ProcessSampler {
        -long clockTicksPerSec
        -uint64_t previousCpuTicks
        -time_point previousSampleTime
        +sample() ProcessSample
    }
    class ProcessSample {
        <<struct>>
        +double cpuUtilization
        +int64_t residentMemoryBytes
    }
    class ProcessMetrics {
        -ProcessSampler sampler
        -ObservableInstrument cpuGauge
        -ObservableInstrument memoryGauge
    }
    ProcessMetrics ..> ProcessSampler : delegates sampling to
    ProcessSampler ..> ProcessSample : produces
    MetricsPipeline ..> ProcessMetrics : composition root builds via Meter
```

Three design points worth calling out:

- `NoOpInterfaceMetrics` and `FakeInterfaceMetrics` both implement
  `IInterfaceMetrics` alongside the real `OtelInterfaceMetrics`. This is
  what fixes the "no test seam" problem described above — any of the three
  can be registered via `setMetrics()` and picked up by every caller of
  `Observability::metrics()`: the OTel-backed one in production, the no-op
  one as the default before `initMetrics()` runs, the fake one in tests
  (scoped with `ScopedMetrics`, see Phase D of the implementation plan).
  `DDSPublisher`/`DDSSubscriber` never see any of the three types directly —
  they only call the interface through the registry accessor.
- The registry (`metrics()`/`setMetrics()`) is two free functions living in
  their own header, `MetricsRegistry.h`. It can't live in `IInterfaceMetrics.h`
  because its default value is a `NoOpInterfaceMetrics`, and
  `IInterfaceMetrics.h` can't depend on a class that implements it without a
  circular include — so the include chain is one-directional:
  `MetricsRegistry.h` includes `NoOpInterfaceMetrics.h`, which includes
  `IInterfaceMetrics.h`. In C++11 (no inline variables), the standard way to
  get one process-wide instance from a header is a function-local `static`
  inside an `inline` function, which is guaranteed to be merged to a single
  definition across translation units by the ODR. This is intentionally
  global, mutable state: it trades per-instance dependency injection for
  zero call-site/constructor changes in `DDSPublisher`/`DDSSubscriber`.
- `ProcessMetrics` has no dependency on `IInterfaceMetrics` or DDS at all —
  it's a sibling of `OtelInterfaceMetrics` under the same `Meter`, not a
  subtype. Process resource usage and interface traffic are separate
  concerns (SRP), and both happen to be implemented with OTel instruments.

## Sequence diagrams

### 1. Process startup (composition root)

```mermaid
sequenceDiagram
    participant Main as main() (Radar.cpp)
    participant MP as MetricsPipeline
    participant MEF as MetricsExporterFactory
    participant SDK as OTel MeterProvider
    participant OIM as OtelInterfaceMetrics
    participant Registry as MetricsRegistry
    participant PM as ProcessMetrics
    participant Pub as DDSPublisher<T>

    Main->>MP: initMetrics(options)
    MP->>MP: buildResource(serviceName)
    MP->>MEF: createMetricExporter(protocol)
    MEF-->>MP: PushMetricExporter
    MP->>SDK: build reader + context, SetMeterProvider(provider)
    SDK-->>MP: shared_ptr<MeterProvider>
    MP->>SDK: meterProvider->GetMeter("Observability")
    SDK-->>MP: Meter
    MP->>OIM: new OtelInterfaceMetrics(meter)
    OIM-->>MP: shared_ptr<IInterfaceMetrics>
    MP->>Registry: setMetrics(oim)
    MP-->>Main: meterProvider
    Main->>PM: new ProcessMetrics(meter)
    Note right of PM: registers cpu/memory ObservableGauge callbacks
    Main->>Pub: new DDSPublisher<T>(domainId, entry, name)
```

`main()` is the **composition root**: it's the only place that knows about
concrete OTel types and wires them together. `initMetrics()` registers the
real recorder into the global registry as its last step, so everything
downstream (`DDSPublisher`, `DDSSubscriber`) can just call
`Observability::metrics()` with no constructor argument and reach the
`IInterfaceMetrics` interface — never a concrete type.

### 2. Publish path

```mermaid
sequenceDiagram
    participant App as Radar main loop
    participant Pub as DDSPublisher<T>
    participant DDS as Cyclone DataWriter
    participant Registry as MetricsRegistry
    participant OIM as OtelInterfaceMetrics
    participant Reader as PeriodicExportingMetricReader
    participant Exp as OTLP/Console Exporter
    participant Collector as otel-collector

    App->>Pub: publish(message)
    Pub->>DDS: writer.write(message)
    Pub->>Registry: metrics()
    Registry-->>Pub: IInterfaceMetrics& (currently OtelInterfaceMetrics)
    Pub->>OIM: recordSent(name, type, topic, bytes)
    OIM->>OIM: messagesSentCounter.Add(1, attrs)
    OIM->>OIM: bytesSentCounter.Add(bytes, attrs)
    Note over Reader,Exp: on the next aggregationPeriod tick
    Reader->>Exp: Export(collected metrics)
    Exp->>Collector: push OTLP (gRPC/HTTP)
```

### 3. Receive path with latency + active-probe gauge

```mermaid
sequenceDiagram
    participant Thread as DDSSubscriber waitLoop (bg thread)
    participant DDS as Cyclone DataReader
    participant Traits as DDSMessageTraits
    participant Registry as MetricsRegistry
    participant OIM as OtelInterfaceMetrics
    participant SDKCollect as OTel SDK collection cycle

    Thread->>DDS: waitSet.wait() / reader.take()
    DDS-->>Thread: samples
    loop each valid sample
        Thread->>Registry: metrics()
        Registry-->>Thread: IInterfaceMetrics&
        Thread->>OIM: recordReceived(name, type, topic, bytes)
        Thread->>Traits: extractLatencySinceSend(sample.data())
        Traits-->>Thread: optional<nanoseconds>
        alt has timestamp header and latency >= 0
            Thread->>OIM: recordLatency(name, type, topic, latency)
            OIM->>OIM: latencyMs.Record(ms, attrs)
        end
        Thread->>Thread: handler(sample.data())
    end

    Note over SDKCollect,OIM: independently, on each collection cycle
    SDKCollect->>OIM: observeActive(ObserverResult, state)
    OIM->>OIM: for each registered probe: Observe(flag->load() ? 1 : 0, attrs)
```

`extractLatencySinceSend` is a compile-time trait
(`has_timestamp_header_v<T>`), not a runtime check — message types without a
`header().timestamp_ns()` (e.g. the DDS test suite's `TestMessage`) simply
never call `recordLatency`, with the branch compiled out via `if constexpr`.
No IDL changes are required for message types that don't opt in.

### 4. Process CPU/memory sampling (pull, no registry needed)

```mermaid
sequenceDiagram
    participant SDKCollect as OTel SDK collection cycle
    participant PM as ProcessMetrics
    participant PS as ProcessSampler
    participant OS as /proc/self/stat, /proc/self/status

    Note over SDKCollect,PM: every aggregationPeriod tick, for each gauge
    SDKCollect->>PM: observeCpu(ObserverResult, state)
    PM->>PS: sample()
    PS->>OS: read utime+stime, VmRSS
    OS-->>PS: raw values
    PS->>PS: cpuUtilization = (cpuTicksDelta / clockTicksPerSec) / wallTimeDelta
    PS-->>PM: ProcessSample{cpuUtilization, residentMemoryBytes}
    PM->>SDKCollect: result.Observe(cpuUtilization, {})
    SDKCollect->>PM: observeMemory(ObserverResult, state)
    PM->>SDKCollect: result.Observe(residentMemoryBytes, {})
```

`ProcessSampler` has no OTel dependency at all — it's pure logic
(read `/proc`, compute a delta) that `ProcessMetrics` wraps with OTel
plumbing. This mirrors the same split used for `OtelInterfaceMetrics` vs.
`MetricsPipeline`: keep SDK wiring separate from the logic that actually
produces a value, so the logic can be unit-tested without an SDK in the
loop.

## Why Views? (histogram bucket boundaries)

OTel's default histogram bucket boundaries are tuned for HTTP-request
durations measured in seconds — wrong for loopback DDS latencies that live
in the microsecond-to-low-millisecond range. Without an explicit View, the
`interface.latency` histogram would bucket essentially all observations
into the same "very fast" bucket, making the histogram useless for
percentile estimation. The design registers one explicit View with
millisecond-scale boundaries (e.g. `{0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10, 25,
50, 100, 250, 500}` ms) via the SDK's `ViewRegistryFactory`/`ViewFactory` —
this is the concrete reason Views exist as a concept: they let you retarget
an instrument's aggregation behavior without changing the instrumentation
call site.

## What's not here yet

Deliberately out of scope for this pass, and worth exploring as natural
next steps once the team wants to go further:

- **Distributed tracing/spans** — linking a span from `Radar`'s `publish()`
  to `Workstation`'s receive handler into one trace would need a
  `traceparent`-equivalent field propagated through the DDS message header
  (an IDL change), plus `Tracer`/`Span` wiring parallel to the metrics
  pipeline here.
- **Baggage propagation** — passing cross-cutting metadata (e.g. a
  correlation ID) across process boundaries via DDS message headers,
  analogous to HTTP baggage headers.
- **Exemplars** — linking histogram bucket observations back to the trace
  ID that produced them, which requires tracing to exist first.
- **Log-trace correlation** — enriching `OtelLogSink` log records with the
  current span's `trace_id`/`span_id`, also requires tracing to exist
  first.
