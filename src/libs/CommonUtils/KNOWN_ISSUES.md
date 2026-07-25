# Known Issues — CommonUtils

## Async logger can crash on shutdown when combined with `Observability::initLogging()`

**Status:** open, not yet fixed. Discovered 2026-07-24 during `GrpcChatLogger`
Phase 2 verification (gRPC native OpenTelemetry plugin); pre-existing and not
caused by that work — see Reproduction below.

### Symptom

Under an ASan build (`debug-san` preset), a process using both
`CommonUtils::GeneralLogger` and `Observability::initLogging()` can crash on
shutdown with:

```
AddressSanitizer:DEADLYSIGNAL
==<pid>==ERROR: AddressSanitizer: SEGV on unknown address ...
    #0 opentelemetry::v1::sdk::logs::Logger::CreateLogRecord()
    #1 opentelemetry::v1::logs::Logger::EmitLogRecord<...>() opentelemetry/logs/logger.h:102
    #2 sink_it_ src/libs/Observability/OtelLogSink.cpp:50
    #3 spdlog::sinks::base_sink<std::mutex>::log(...)
    #4 spdlog::async_logger::backend_sink_it_(...)
    #5 spdlog::details::thread_pool::process_next_msg_()
    #6 spdlog::details::thread_pool::worker_loop_()
```

The crash happens on spdlog's async background worker thread, **after** the
process has already logged its own "shutting down" and "General Logger
Destructor" messages on the main thread — i.e. after `main()` has returned
and the process has entered static/global teardown.

### Root cause

- `GeneralLogger::s_generalLogger` / `s_traceLogger`
  ([GeneralLogger.cpp:19-20](GeneralLogger.cpp)) are `static` class members —
  process-lifetime objects backed by a process-lifetime async worker thread
  (`spdlog::init_thread_pool`, [GeneralLogger.cpp:37](GeneralLogger.cpp)).
  The *local* `CommonUtils::GeneralLogger` object apps construct in `main()`
  does **not** own or stop this thread pool on destruction — its destructor
  ([GeneralLogger.cpp:22-27](GeneralLogger.cpp)) only logs a message and
  dumps the trace backtrace.
- `Observability::initLogging()`'s `LoggerProvider` is kept alive past the
  app's own local `shared_ptr` by a second reference held inside
  OpenTelemetry's own global `Provider` registry
  (`logs_api::Provider::SetLoggerProvider(apiProvider)`,
  [OtelLogSink.cpp:85](../Observability/OtelLogSink.cpp)). That means the
  `LoggerProvider` isn't actually destroyed when the app's local
  `otelLoggerProvider` variable goes out of scope at the end of `main()`
  either — it's destroyed later, whenever OpenTelemetry's own internal
  static storage is torn down.
- Both of these therefore get destroyed during the same general
  **post-`main()` static-destruction phase** — and C++ gives no guaranteed
  ordering between static/global destructors that live in different
  translation units or libraries (the classic "static deinitialization
  order fiasco," here between spdlog's internals and OpenTelemetry's
  internals).
- If the async worker thread is still processing a queued log message when
  OpenTelemetry's statics are torn down first, `OtelLogSink::sink_it_`
  ([OtelLogSink.cpp:49](../Observability/OtelLogSink.cpp)) calls
  `opentelemetry::logs::Provider::GetLoggerProvider()` into an
  already-destroyed provider — the SEGV above.

### Reproduction

Confirmed with a real `docker build` + the `debug-san` preset:

- `src/apps/GrpcChatLogger/{ChatLoggerServer,ChatLoggerReporter,ChatLoggerReader}` —
  all four running processes (server + reporter + 2 readers) crash with the
  above ASan SEGV on `SIGTERM`, reliably, when there has been continuous log
  traffic (one `GPINFO` roughly every 2s from the reporter) right up to the
  signal.
- `src/apps/RadarDDSDemo/RadarDDSRadar` — built with the **identical**
  `debug-san` image, sent the same `SIGTERM` — shuts down cleanly, no crash.
  The difference is log volume near shutdown, not the app itself: by the
  time `RadarDDSRadar`'s test run reached shutdown its logging had gone
  quiet for a couple of seconds, so the single async worker thread had
  already drained the queue before the process actually exited.

This means the bug is latent in **every** app using the
`CommonUtils::GeneralLogger` + `Observability::initLogging()` +
`Observability::createOtelLogSink()` combination
(`src/apps/RadarDDSDemo`, `src/apps/GrpcChatLogger`) — it just needs enough
log volume close to shutdown to reliably surface, which `GrpcChatLogger`'s
continuous RPC traffic happens to produce and `RadarDDSDemo`'s bursty
publish pattern usually doesn't.

### Possible fix directions (not yet implemented)

- Have `GeneralLogger`'s destructor (or a new explicit `shutdown()` method
  apps call before tearing down the `Observability` providers) drain and
  stop the async thread pool — e.g. `spdlog::shutdown()` — *before*
  `Observability::init`/`initLogging`'s returned providers are destroyed,
  so no queued message can outlive the `LoggerProvider`/`MeterProvider` it
  targets.
- Alternatively, have `OtelLogSink::sink_it_` guard against a torn-down/null
  provider (defensive null-check) rather than relying on shutdown ordering
  at all.
- Either fix needs to preserve the existing `ForceFlush()`-not-`Shutdown()`
  behavior documented at each app's shutdown call site (see e.g.
  `src/apps/RadarDDSDemo/Radar.cpp`'s comment on why `Shutdown()` is
  intentionally not called there).
