# Omniscope Design

Omniscope is a web-based IPC traffic inspector that lets you monitor,
record, and replay messages flowing through publish-subscribe middleware
in real time from a browser.

## Technology Stack

| Component                   | Technology                                                       | Purpose                                                                                                                                                  |
| --------------------------- | ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **HTTP / WebSocket server** | [Crow](https://crowcpp.org/) 1.3                                 | Lightweight C++ micro-framework (header-only, BSD-3). Serves the single-page UI and provides the `/ws` WebSocket endpoint for live message streaming.    |
| **DDS middleware**          | [Eclipse Cyclone DDS](https://cyclonedds.io/) 0.10               | OMG Data Distribution Service implementation used by the default transport. Subscribers poll DDS topics for `SensorReading` and `TrackUpdate` IDL types. |
| **Logging**                 | [spdlog](https://github.com/gabime/spdlog) (via `GeneralLogger`) | Async structured logging throughout the application.                                                                                                     |
| **Build**                   | CMake 3.25+ with presets                                         | The HTML UI is embedded at build time via `EmbedHtml.cmake` so the binary is fully self-contained — no external files needed at runtime.                 |
| **Language**                | C++20                                                            | Uses `std::format`, `std::atomic`, `std::jthread`-style patterns, and concepts from the C++20 standard.                                                  |
| **Compatibility**           | `CrowCompat.h`                                                   | Shim that resolves a `std::atomic` / `operator<<` ambiguity when compiling Crow with C++20 / libc++.                                                     |

## Architecture

```mermaid
graph TD
    subgraph Browser
        UI[Single-Page UI<br/>monitor.html]
    end

    subgraph "Omniscope Process"
        App[OmniscopeApp<br/>Crow HTTP + WS]
        PE[PlaybackEngine]
        DT[TransportDds]
        ZT[TransportZyre]

        App -- "subscribe / unsubscribe<br/>record / playback" --> DT
        App -- "subscribe / unsubscribe<br/>record / playback" --> ZT
        App -- "start / stop / load" --> PE
        PE -- "publish callback" --> App
        App -- "route to transport" --> DT
        App -- "route to transport" --> ZT
    end

    subgraph "DDS Domain"
        SP[SensorTopic]
        TP[TrackTopic]
    end

    subgraph "Zyre Network"
        ZP[Zyre Peers]
    end

    UI -- "HTTP GET /" --> App
    UI -- "WebSocket /ws" --> App
    UI -- "POST /playback/load" --> App
    DT -- "DDSSubscriber poll" --> SP
    DT -- "DDSSubscriber poll" --> TP
    DT -- "DDSPublisher write" --> SP
    DT -- "DDSPublisher write" --> TP
    ZT -- "ZyreSubscriber" --> ZP
    ZT -- "ZyrePublisher" --> ZP
```

## Class Diagram

```mermaid
classDiagram
    class ITransport {
        <<interface>>
        +name() string
        +topicNames() vector~string~
        +subscribe(topic, callback)
        +unsubscribe(topic)
        +isSubscribed(topic) bool
        +publishFromJson(topic, jsonData)
    }

    class TransportDds {
        -Impl* _impl
        +TransportDds(domainId)
        +subscribe(topic, callback)
        +unsubscribe(topic)
        +publishFromJson(topic, jsonData)
    }

    class TransportZyre {
        -Impl* _impl
        +TransportZyre(zyreNamespace)
        +subscribe(topic, callback)
        +unsubscribe(topic)
        +publishFromJson(topic, jsonData)
    }

    class OmniscopeApp {
        -Impl* _impl
        +OmniscopeApp(httpPort)
        +addTransport(unique_ptr~ITransport~)
        +run()
    }

    class PlaybackEngine {
        -PublishCallback _publish
        -vector~string~ _lines
        -thread _thread
        -condition_variable _stopCv
        +PlaybackEngine(PublishCallback)
        +loadRecording(body) size_t
        +start(onProgress, onComplete)
        +stop()
        +isPlaying() bool
    }

    ITransport <|.. TransportDds : implements
    ITransport <|.. TransportZyre : implements
    OmniscopeApp o-- ITransport : transports
    OmniscopeApp *-- PlaybackEngine
    PlaybackEngine --> OmniscopeApp : publishes via callback
```

## Data Flow

### Live Monitoring

```mermaid
sequenceDiagram
    participant B as Browser
    participant App as OmniscopeApp
    participant DT as TransportDds
    participant ZT as TransportZyre
    participant DDS as DDS Domain
    participant ZN as Zyre Network

    B->>App: WebSocket {"type":"subscribe","topic":"SensorTopic"}
    App->>DT: subscribe("SensorTopic", callback)
    DT->>DDS: DDSSubscriber.start() — poll loop
    DDS-->>DT: SensorReading sample
    DT-->>App: callback(topic, json)
    App-->>B: WebSocket {"type":"message","topic":"SensorTopic","data":{...}}

    B->>App: WebSocket {"type":"subscribe","topic":"SensorReading"}
    App->>ZT: subscribe("SensorReading", callback)
    ZT->>ZN: ZyreSubscriber listen
    ZN-->>ZT: protobuf message
    ZT-->>App: callback(topic, json)
    App-->>B: WebSocket {"type":"message","topic":"SensorReading","data":{...}}
```

### Recording & Playback

```mermaid
sequenceDiagram
    participant B as Browser
    participant App as OmniscopeApp
    participant PE as PlaybackEngine

    Note over B,App: Recording
    B->>App: {"type":"record_start"}
    App-->>App: Open .dat file, flag recording=true
    Note right of App: Each incoming message is<br/>appended as JSON-Lines

    B->>App: {"type":"record_stop"}
    App-->>B: {"type":"recording_stopped","filename":"omni_*.dat"}

    Note over B,PE: Playback
    B->>App: POST /playback/load (file body)
    App->>PE: loadRecording(body)
    B->>App: {"type":"playback_start"}
    App->>PE: start(onProgress, onComplete)
    PE->>App: publish callback(topic, jsonData)
    App->>App: route to originating transport
    PE-->>App: onProgress(current, total)
    App-->>B: {"type":"playback_progress","current":42,"total":100}
    PE-->>App: onComplete(false)
    App-->>B: {"type":"playback_finished"}
```

## Shutdown Sequence

```mermaid
flowchart TD
    SIG["SIGINT / SIGTERM"] --> FLAG["isRunning = false"]
    FLAG --> EXIT["Main loop exits"]
    EXIT --> STOP_PB["playback.stop()<br/>notify condition_variable → join thread"]
    STOP_PB --> STOP_CROW["crowApp.stop()<br/>close HTTP + WebSocket listeners"]
    STOP_CROW --> DTOR["~OmniscopeApp"]
    DTOR --> DTOR_PB["~PlaybackEngine — stop() (no-op)"]
    DTOR --> DTOR_CROW["~crow::SimpleApp"]
    DTOR --> DTOR_DT["~TransportDds<br/>stop DDS subscribers"]
    DTOR --> DTOR_ZT["~TransportZyre<br/>stop Zyre subscriber/publisher"]
```

Crow's built-in signal handlers are cleared via `signal_clear()` before
`run_async()` so that the application's own `std::signal` handlers
remain in control. The `PlaybackEngine` uses a `std::condition_variable`
for its inter-message sleep so that `stop()` wakes it immediately
rather than blocking up to 5 seconds.

## Transport Extensibility

Omniscope is designed around the `ITransport` interface. Two transports
are included: `TransportDds` (Cyclone DDS) and `TransportZyre` (Zyre +
protobuf). Adding a new transport (e.g. ZMQ, MQTT) requires:

1. Create a class that implements `ITransport`.
2. Instantiate it in `main.cpp` and register via `app.addTransport()`.
3. Omniscope discovers topics automatically via `topicNames()`.

No changes to `OmniscopeApp`, `PlaybackEngine`, or the web UI are
necessary.

## WebSocket Protocol

All browser↔server communication (except the initial page load and file
upload) flows over a single WebSocket at `/ws`. Messages are JSON
objects with a `"type"` field:

### Client → Server

| Type             | Fields  | Description                               |
| ---------------- | ------- | ----------------------------------------- |
| `subscribe`      | `topic` | Start receiving live messages for a topic |
| `unsubscribe`    | `topic` | Stop receiving messages for a topic       |
| `record_start`   | —       | Begin recording incoming messages to disk |
| `record_stop`    | —       | Stop recording                            |
| `playback_start` | —       | Start replaying the loaded recording      |
| `playback_stop`  | —       | Stop an in-progress playback              |

### Server → Client

| Type                | Fields                       | Description                                                                               |
| ------------------- | ---------------------------- | ----------------------------------------------------------------------------------------- |
| `topics`            | `topics[]`                   | Full topic list with subscription state (sent on connect and after subscribe/unsubscribe) |
| `message`           | `topic`, `timestamp`, `data` | A live or replayed IPC message                                                            |
| `recording_started` | —                            | Acknowledge recording start                                                               |
| `recording_stopped` | `filename`                   | Acknowledge recording stop, return filename                                               |
| `recording_loaded`  | `count`                      | A recording file was uploaded and parsed                                                  |
| `playback_started`  | `count`                      | Playback has begun                                                                        |
| `playback_progress` | `current`, `total`           | Periodic progress update                                                                  |
| `playback_finished` | —                            | Playback completed normally                                                               |
| `playback_stopped`  | —                            | Playback was stopped by the user                                                          |

## HTTP Endpoints

| Method | Path             | Description                                  |
| ------ | ---------------- | -------------------------------------------- |
| `GET`  | `/`              | Serves the embedded single-page HTML UI      |
| `POST` | `/playback/load` | Upload a `.dat` JSON-Lines file for playback |

## Recording File Format

Recordings are stored as JSON-Lines (`.dat`), one message per line:

```json
{"topic":"SensorTopic","timestamp":"2026-03-14T12:00:00.123Z","timestamp_ms":1773576000123,"data":{...}}
```

## File Layout

```
src/apps/Omniscope/
├── CMakeLists.txt          # Build config, HTML embedding, link Crow + CycloneDDS + PubSubLib + ProtoLib
├── CrowCompat.h            # C++20 / libc++ compatibility shim for Crow
├── ITransport.h            # Abstract transport interface
├── TransportDds.h/.cpp     # Cyclone DDS transport (pImpl)
├── TransportZyre.h/.cpp    # Zyre protobuf transport (pImpl)
├── PlaybackEngine.h/.cpp   # Recording playback with original timing
├── OmniscopeApp.h/.cpp     # Crow HTTP/WS orchestrator (pImpl)
├── main.cpp                # Entry point, argument parsing
└── web/
    └── monitor.html        # Single-page browser UI (embedded at build time)
```

## Usage

```bash
# Build
cmake --build --preset container-debug

# Run with defaults (domain 0, port 8080, Zyre namespace "TestZyre")
./build/container-debug/bin/Omniscope

# Run with custom DDS domain and HTTP port
./build/container-debug/bin/Omniscope 1 9090

# Run with custom Zyre namespace
./build/container-debug/bin/Omniscope 0 8080 MyNamespace

# Open in browser
xdg-open http://localhost:8080
```

From the browser UI you can:

1. **Subscribe** to any available topic to see live messages.
2. **Record** traffic to a `.dat` file.
3. **Upload** a previous recording and **play it back** through the
   transport, re-publishing each message with original timing.
4. **Stop** playback at any time.

Press **Ctrl+C** in the terminal to shut down cleanly.
