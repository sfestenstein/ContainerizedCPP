# Project Design

This document describes the architecture and design decisions of the ContainerizedCPP project.

## Overview

ContainerizedCPP is a focused showcase of DDS tooling: a web-based DDS
traffic inspector with dynamic topic discovery, recording, and playback
(OmniscopeDds), plus a two-app QoS demonstration (RadarDDSDemo). Everything
runs on a single DDS implementation — Eclipse Cyclone DDS.

## Architecture

### Component Diagram

```
┌───────────────────────────────────────────────────────────────────────┐
│                              Applications                             │
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │                          OmniscopeDds                             │  │
│  │  OmniscopeApp (Crow HTTP/WS server) + PlaybackEngine + web UI;   │  │
│  │  TransportDds discovers topics dynamically via                   │  │
│  │  BuiltinTopicReader + BlobSertype (no fixed topic set), and      │  │
│  │  replays recorded raw CDR bytes verbatim for playback            │  │
│  └─────────────────────────────────────────────────────────────────┘  │
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │                     RadarDDSDemo (Radar / Workstation)            │  │
│  │  Own IDL (5 topics), per-topic QoS profile demonstration          │  │
│  │  (Reliable/BestEffort, Volatile/TransientLocal, KeepLast/KeepAll)│  │
│  └─────────────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────────┤
│                                Libraries                              │
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │                    CycloneDDS Library (STATIC)                    │  │
│  │  ┌────────────────┐  ┌────────────────┐  ┌─────────────────┐    │  │
│  │  │ DDSTopicConfig │  │ DDSPublisher<T>│  │ DDSSubscriber<T>│    │  │
│  │  │ (QoS registry) │  │ (header-only)  │  │(header-only,    │    │  │
│  │  │                │  │                │  │ WaitSet-driven) │    │  │
│  │  └────────────────┘  └────────────────┘  └─────────────────┘    │  │
│  │  ┌────────────────────┐  Type-agnostic: each app supplies its    │  │
│  │  │ BlobSertype /       │  own IDL and generates its own message  │  │
│  │  │ BuiltinTopicReader  │  types (see RadarDDSDemo, DDSTests)     │  │
│  │  │ (OmniscopeDds-only) │                                         │  │
│  │  └────────────────────┘                                          │  │
│  └─────────────────────────────────────────────────────────────────┘  │
│  ┌─────────────────┐                                                  │
│  │   CommonUtils   │                                                  │
│  │  (logging, timers, utilities)                                     │
│  └─────────────────┘                                                  │
├───────────────────────────────────────────────────────────────────────┤
│                          External Dependencies                        │
│  ┌────────┐        ┌──────────┐        ┌────────┐                    │
│  │ spdlog │        │ Cyclone  │        │  Crow  │                    │
│  │        │        │   DDS    │        │        │                    │
│  └────────┘        └──────────┘        └────────┘                    │
└───────────────────────────────────────────────────────────────────────┘
```

### Libraries

#### CommonUtils Library (`src/libs/CommonUtils/`)

The CommonUtils library provides reusable components for common tasks:

- **GeneralLogger**: An async logging wrapper around spdlog providing:
  - Dual-logger system (general + trace)
  - Convenience macros (GPCRIT, GPERROR, GPWARN, GPINFO, GPDEBUG, GPTRACE)
  - Async logging with configurable queue size
  - Thread-safe initialization

- **Timer**: A basic timer class:
  - Periodic and single-shot modes
  - Callback-based design
  - Thread-safe start/stop operations
  - Millisecond precision

- **SnoozableTimer**: An extended timer with snooze capability:
  - Inherits from Timer
  - Snooze functionality to extend timeout
  - Useful for implementing watchdog patterns

- **DataHandler**: Data handling utilities (header-only):
  - Template-based data processing
  - Flexible data transformation support

#### CycloneDDS Library (`src/libs/CycloneDDS/`)

The CycloneDDS library provides topic-based DDS publish-subscribe via Eclipse Cyclone DDS. The CMake target (`CycloneDDSLib`) is a STATIC library of header-only template publishers/subscribers plus dynamic-discovery support. It is type-agnostic — it ships no message IDL of its own; each app supplies its own IDL and generates its own message types via `idlcxx_generate()` (see `src/apps/RadarDDSDemo/CMakeLists.txt` or `tests/DDSTests/CMakeLists.txt` for the pattern).

- **DDSTopicConfig**: Central registry mapping topic names to `DataWriterQos` and `DataReaderQos`, guaranteeing RxO (Request-vs-Offered) compatibility between publishers and subscribers.

- **DDSPublisher\<T\>**: Template publisher that lazily creates `DataWriter` instances per topic. QoS is looked up from `DDSTopicConfig` automatically.

- **DDSSubscriber\<T\>**: Template subscriber with a background thread that delivers samples event-driven rather than polled: it blocks on a `dds::core::cond::WaitSet` attached to the reader's `StatusCondition` (`data_available`), so it wakes immediately when a sample arrives instead of on a fixed sleep interval. A `GuardCondition` on the same WaitSet lets `stop()` wake the thread immediately too, rather than waiting out a timeout.

- **BlobSertype** / **BuiltinTopicReader**: Support dynamic, type-unaware topic discovery and raw-CDR capture/replay — used only by OmniscopeDds. `BuiltinTopicReader` polls CycloneDDS's built-in `DCPSPublication` topic to discover active publishers/topics at runtime; `BlobSertype` is a custom `ddsi_sertype` that accepts arbitrary CDR bytes without a compiled-in IDL type, letting OmniscopeDds subscribe to (and publish onto) topics it has no generated type for.

### Applications

#### OmniscopeDds (`src/apps/OmniscopeDds/`)

A web-based DDS traffic inspector with dynamic topic discovery, recording, and playback:
- **ITransport** — abstract transport interface decoupling the monitor from any specific DDS implementation; the seam a future richer DDS transport could plug into without touching `OmniscopeApp`
- **OmniscopeApp** — orchestrates transports, Crow HTTP/WebSocket server, recording, and playback (pImpl pattern)
- **PlaybackEngine** — loads `.dat` files and replays them in a background thread with original inter-message timing (capped at 5 s per gap), publishing via a routing callback that dispatches to the originating transport
- **TransportDds** — joins a CycloneDDS domain and discovers *any* active topic at runtime via `CycloneDDS::BuiltinTopicReader`, rather than a fixed, compiled-in topic set. Subscribes to arbitrary topics using `CycloneDDS::BlobSertype`, delivering raw CDR bytes hex-encoded as JSON (`{"raw_cdr":"...","byte_count":N,"type_name":"..."}`) — no IDL types need to be known ahead of time. Fires a topics-changed callback when publishers appear or disappear on the domain.
- **Embedded HTML UI** — dark-theme 3-pane interface (Topics / Messages / Detail) served at `/`, with WebSocket streaming, recording controls, and load/playback with a progress bar

**Playback / replay**: `publishFromJson()` replays a recorded message by wire-level byte replay rather than general JSON→CDR encoding — since the captured JSON already contains the exact raw CDR bytes (`raw_cdr`), there's no need to re-derive them from a compiled type. It hex-decodes `raw_cdr`, lazily creates a `BlobSertype`-based writer for the topic (Reliable/Volatile/KeepLast(1) QoS — Reliable is compatible with both Reliable- and BestEffort-requesting readers per DDS RxO rules), builds a `ddsi_serdata` from the decoded bytes via `ddsi_serdata_from_ser_iov()`, and publishes it with `dds_writecdr()`. Verified end-to-end: an independent subscriber observed the replayed bytes match the recorded ones exactly. One caveat inherent to DDS, not this implementation: a sample published immediately after a brand-new writer is created can be lost if the reader hasn't finished matching yet — this only affects the very first replayed sample after a topic's writer is first created, not steady-state playback.

Usage:
```bash
./build/debug-san/bin/OmniscopeDds [domain_id] [http_port]
# Open http://localhost:8080 (default port)
```

#### RadarDDSDemo (`src/apps/RadarDDSDemo/`)

A two-app demonstration of Cyclone DDS QoS profiles, with its own IDL (`RadarMessageHeader`, `Command`, `CommandStatus`, `RadarTrack`, `ComponentStatus`, `RadarAlert`):

- **RadarDDSRadar** (`Radar.cpp`) — simulates a radar sensor node: publishes `RadarTrack` (Best Effort/Volatile/KeepLast(1)), `ComponentStatus` (Reliable/TransientLocal/KeepLast(1), periodic), and `RadarAlert` (Reliable/TransientLocal/KeepAll, rare); subscribes to `Command` and replies with `CommandStatus`. A `--stress` flag fires an unthrottled burst of `RadarTrack` samples at startup to force observable Best-Effort drops.
- **RadarDDSWorkstation** (`Workstation.cpp`) — simulates an operator workstation: sends `Command` periodically, subscribes to `CommandStatus`/`RadarTrack`/`ComponentStatus`/`RadarAlert`, and demonstrates the practical effect of each QoS profile — tracking per-track sequence gaps for Best-Effort drops, and tagging the first `ComponentStatus`/`RadarAlert` sample as a "late-joiner snapshot" to show TransientLocal durability.
- **RadarTopics** — builds a `CycloneDDS::DDSTopicConfig` registry for the 5 topics with explicit per-topic QoS.

OmniscopeDds's dynamic discovery picks up RadarDDSDemo's topics automatically with zero extra code, which is the natural way to pair the two for a demo.

Usage:
```bash
./build/debug-san/bin/RadarDDSRadar [domain_id] [--stress]
./build/debug-san/bin/RadarDDSWorkstation [domain_id]
```

## Design Decisions

### Build System

**CMake** was chosen as the build system because:
- Industry standard for C++ projects
- Excellent IDE integration
- Works well with containerized Linux toolchains
- Modern features (presets, toolchain files)

**Containerized + system package dependency management** was chosen because:
- Reproducible Linux build environment via Docker/Dev Containers
- Simpler CI and local setup without per-user package cache/toolchain state
- Fast incremental iteration with CMake presets
- One supported toolchain path for consistent developer and CI behavior

### Code Quality

**clang-format** ensures consistent code style:
- 3-space indentation
- Allman brace style (braces on new line)
- 100-character line limit

**clang-tidy** provides static analysis:
- Modern C++ best practices
- Bug detection
- Performance suggestions
- Naming conventions

### Testing Strategy

**Google Test** was chosen because:
- Widely used in industry
- Feature-rich (fixtures, mocking, parameterized tests)
- Good IDE integration
- Clear test output

Test organization:
- One test file per source file
- Tests mirror the source structure
- Fixtures for common setup/teardown

### Sanitizers

Address Sanitizer (ASan) and Undefined Behavior Sanitizer (UBSan) are enabled in debug builds to catch:
- Memory leaks
- Buffer overflows
- Use-after-free
- Undefined behavior

### Code Coverage

Coverage is collected using gcov/lcov:
- Line and branch coverage
- HTML report generation
- CI integration with Codecov

## Future Considerations

Areas for potential enhancement:

1. **Benchmarking**: Add Google Benchmark for performance testing
2. **Documentation**: Add Doxygen for API documentation
3. **Packaging**: Add CPack for installers/packages
4. **Cross-compilation**: Add toolchain files for embedded targets
5. **Fuzzing**: Add libFuzzer for fuzz testing
