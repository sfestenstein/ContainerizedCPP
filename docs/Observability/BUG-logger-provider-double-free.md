# Bug: double-free in `LoggerProvider` destructor at shutdown

**Status:** Open, pre-existing, not caused by the Phase A metrics refactor.
**Severity:** Low in practice (ASan-only; not observed to crash non-ASan
builds), but a real double-free.

## Symptom

Under the `debug-san` preset (AddressSanitizer), `RadarDDSRadar` (and
presumably `RadarDDSWorkstation`) aborts with a double-free during process
shutdown, after the "shutting down" log line:

```
==<pid>==ERROR: AddressSanitizer: attempting double-free on 0x... in thread T1:
    #0 operator delete(void*, unsigned long) ...
    #1 opentelemetry::v1::sdk::logs::LoggerProvider::~LoggerProvider() ...
    #2 std::default_delete<opentelemetry::v1::logs::LoggerProvider>::operator()() ...
    ...
```

The second stack (allocation site) points at the OTel logs SDK's internal
`std::vector<std::shared_ptr<Logger>>` growing when `GetLogger("GeneralLogger")`
is called from `OtelLogSink::sink_it_()`.

## Reproduction

```
cd build/debug-san
./bin/RadarDDSRadar &
sleep 3
kill -TERM %1   # or Ctrl+C
```

Crashes reliably within a few seconds of startup, on shutdown.

## Confirmed pre-existing

Reproduced identically on an isolated worktree of the pre-session baseline
commit (`f2793b6`, before any Observability redesign work started) — same
double-free, same stack, same location. This is **not** a regression from
the Phase A `MetricsRegistry`/`OtelInterfaceMetrics`/`MetricsPipeline` split;
Phase A does not touch `OtelLogSink.h`/`.cpp` or logging pipeline
construction at all.

## Likely cause

`Radar.cpp`/`Workstation.cpp` already carry a comment flagging this exact
class of hazard:

> Shutdown() is intentionally not called here: the SDK's
> MeterProvider/LoggerProvider destructors already shut themselves down
> exactly once when the last reference (held by the global
> opentelemetry::metrics::Provider/opentelemetry::logs::Provider) is
> released at process exit, and calling Shutdown() a second time there is
> undefined behavior.

The double-free is consistent with a static-destruction-order problem
between the global `opentelemetry::logs::Provider`'s held `shared_ptr<LoggerProvider>`
and gRPC's own global state (used transitively by the OTLP/gRPC log
exporter) — both are torn down at process exit in an order the SDK doesn't
fully control, and something in that path frees the same block twice.

## Scope / next step

Out of scope for the current Observability redesign pass per project
priorities — `OtelLogSink` and the logging pipeline are deliberately
deferred (see `IMPLEMENTATION_PLAN.md`). Revisit when logging gets its own
SOLID pass; likely candidates to investigate then:

- Whether `LoggerProvider::Shutdown()` should be called explicitly before
  `ForceFlush()`-then-return, instead of relying on destruction order.
- Whether the OTLP/gRPC log exporter needs to be destroyed before gRPC's
  own global shutdown runs (ordering between `Observability::initLogging()`'s
  returned provider and any gRPC-internal static state).
- Whether switching the logging exporter to OTLP/HTTP (avoiding gRPC's
  global state entirely) sidesteps the issue.
