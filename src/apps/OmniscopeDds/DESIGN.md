# OmniscopeDds Design

OmniscopeDds is a web-based DDS traffic inspector that lets you monitor,
record, and replay messages flowing through a Cyclone DDS domain in real
time from a browser, with fully dynamic topic discovery — no compiled-in
IDL types or fixed topic list required.

It is the result of merging two earlier apps: Omniscope (the web UI,
recording, and playback engine) and DdsSnooper (dynamic DDS topic
discovery via CycloneDDS's built-in discovery topics). All of DdsSnooper's
capability is preserved, plus playback now actually re-publishes onto the
DDS domain instead of being a stub.

## Technology Stack

| Component                   | Technology                                                       | Purpose                                                                                                                                          |
| --------------------------- | ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **HTTP / WebSocket server** | [Crow](https://crowcpp.org/) 1.3                                 | Lightweight C++ micro-framework (header-only, BSD-3). Serves the single-page UI and provides the `/ws` WebSocket endpoint for live message streaming.    |
| **DDS middleware**          | [Eclipse Cyclone DDS](https://cyclonedds.io/) 0.10               | OMG Data Distribution Service implementation used by `TransportDds`. Topics are discovered dynamically at runtime — no compiled-in IDL types.            |
| **Logging**                 | [spdlog](https://github.com/gabime/spdlog) (via `GeneralLogger`) | Async structured logging throughout the application.                                                                                                     |
| **Build**                   | CMake 3.25+ with presets                                         | The HTML/CSS/JS are embedded at build time via `EmbedAsset.cmake` so the binary is fully self-contained — no external files needed at runtime.            |
| **Language**                | C++20                                                            | Uses `std::format`, `std::atomic`, and concepts from the C++20 standard.                                                                                  |

## Architecture

```mermaid
graph TD
    subgraph Browser
        UI[Single-Page UI<br/>monitor.html]
    end

    subgraph "OmniscopeDds Process"
        App[OmniscopeApp<br/>Crow HTTP + WS]
        PE[PlaybackEngine]
        DT[TransportDds]

        App -- "subscribe / unsubscribe<br/>record / playback" --> DT
        App -- "start / stop / load" --> PE
        PE -- "publish callback" --> App
        App -- "route to transport" --> DT
    end

    subgraph "DDS Domain"
        BT["DCPSPublication<br/>(built-in discovery topic)"]
        AnyTopic["Any discovered topic<br/>(no fixed set)"]
    end

    UI -- "HTTP GET /" --> App
    UI -- "WebSocket /ws" --> App
    UI -- "POST /playback/load" --> App
    DT -- "BuiltinTopicReader poll" --> BT
    DT -- "BlobSertype reader (raw CDR)" --> AnyTopic
    DT -- "BlobSertype writer (replay)" --> AnyTopic
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
        +setTopicsChangedCallback(callback)
    }

    class TransportDds {
        -Impl* _impl
        +TransportDds(domainId)
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
    participant DDS as DDS Domain

    B->>App: WebSocket {"type":"subscribe","topic":"RadarTrack"}
    App->>DT: subscribe("RadarTrack", callback)
    DT->>DDS: dds_takecdr() poll loop (per-topic thread)
    DDS-->>DT: raw CDR bytes
    DT-->>App: callback(topic, {"raw_cdr":"...","type_name":"..."})
    App-->>B: WebSocket {"type":"message","topic":"RadarTrack","data":{...}}
```

### Recording & Playback (Wire-Level Replay)

```mermaid
sequenceDiagram
    participant B as Browser
    participant App as OmniscopeApp
    participant PE as PlaybackEngine
    participant DT as TransportDds
    participant DDS as DDS Domain

    Note over B,App: Recording
    B->>App: {"type":"record_start"}
    App-->>App: Open .dat file, flag recording=true
    Note right of App: Each incoming message (including<br/>raw_cdr hex) is appended as JSON-Lines

    B->>App: {"type":"record_stop"}
    App-->>B: {"type":"recording_stopped","filename":"omni_*.dat"}

    Note over B,PE: Playback
    B->>App: POST /playback/load (file body)
    App->>PE: loadRecording(body)
    B->>App: {"type":"playback_start"}
    App->>PE: start(onProgress, onComplete)
    PE->>App: publish callback(topic, jsonData)
    App->>DT: publishFromJson(topic, jsonData)
    DT->>DT: hex-decode raw_cdr, get-or-create writer for topic
    DT->>DDS: dds_writecdr() — publishes the exact recorded bytes
    PE-->>App: onProgress(current, total)
    App-->>B: {"type":"playback_progress","current":42,"total":100}
    PE-->>App: onComplete(false)
    App-->>B: {"type":"playback_finished"}
```

**Wire-level replay, not JSON re-encoding.** Since the JSON captured for
each message already contains the raw CDR bytes (`raw_cdr`, hex-encoded),
`publishFromJson()` doesn't need to know the compiled type to re-encode a
sample — it hex-decodes the bytes and publishes them verbatim via
`ddsi_serdata_from_ser_iov()` + `dds_writecdr()`, using a lazily-created
`BlobSertype`-based writer (Reliable/Volatile/KeepLast(1) QoS — Reliable
because it's compatible with both Reliable- and BestEffort-requesting
readers per DDS RxO rules). This was verified end-to-end: an independent
subscriber, already matched with the writer, received the replayed bytes
byte-for-byte identical to the original recording. The one caveat inherent
to DDS itself (not specific to this implementation): the very first sample
published on a brand-new writer can be lost if the reader hasn't finished
matching yet — this doesn't affect steady-state playback of multiple
samples on an already-active topic.

## Shutdown Sequence

`OmniscopeApp::run()` blocks in Crow's `.multithreaded().run()`, which
handles SIGINT/SIGTERM itself and returns when signaled — there is no
explicit `signal_clear()`/`crowApp.stop()` call. After `run()` returns,
`OmniscopeApp::run()` calls `playback->stop()`, and `~OmniscopeApp()` calls
it again (idempotent, harmless). Member destruction then proceeds in
reverse declaration order: `playback` is declared before `transports` in
`OmniscopeApp::Impl`, so `PlaybackEngine` is destroyed *before*
`TransportDds` — meaning no in-flight playback can race with `TransportDds`
tearing down its participant/readers/writers.

`PlaybackEngine` uses a `std::condition_variable` for its inter-message
sleep so that `stop()` wakes it immediately rather than blocking up to 5
seconds.

## Transport Extensibility

OmniscopeDds is designed around the `ITransport` interface. Currently only
`TransportDds` (Cyclone DDS, dynamic discovery) is included, but the
interface is implementation-agnostic — a future richer DDS implementation,
or an entirely different pub/sub middleware, can be added as a new
transport without touching `OmniscopeApp`. Adding a new transport requires:

1. Create a class that implements `ITransport`.
2. Instantiate it in `main.cpp` and register via `app.addTransport()`.
3. OmniscopeDds discovers topics automatically via `topicNames()`.

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
| `message`           | `topic`, `timestamp`, `data` | A live or replayed message; `data` includes `raw_cdr`, `byte_count`, `type_name`          |
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
| `GET`  | `/style.css`     | Serves the embedded stylesheet               |
| `GET`  | `/app.js`        | Serves the embedded client-side JavaScript   |
| `POST` | `/playback/load` | Upload a `.dat` JSON-Lines file for playback |

## Recording File Format

Recordings are stored as JSON-Lines (`.dat`), one message per line:

```json
{"topic":"RadarTrack","timestamp":"2026-03-14T12:00:00.123Z","timestamp_ms":1773576000123,"data":{"raw_cdr":"...","byte_count":84,"type_name":"radar_demo::RadarTrack"}}
```

## File Layout

```
src/apps/OmniscopeDds/
├── CMakeLists.txt          # Build config, web asset embedding, link Crow + CommonUtils + CycloneDDSLib
├── ITransport.h            # Abstract transport interface
├── PlaybackEngine.h/.cpp   # Recording playback with original timing
├── OmniscopeApp.h/.cpp     # Crow HTTP/WS orchestrator (pImpl)
├── TransportDds.h/.cpp     # Dynamic DDS discovery + raw-CDR capture/replay (pImpl)
├── main.cpp                # Entry point, argument parsing
└── web/
    ├── monitor.html        # Single-page browser UI markup
    ├── style.css            # Stylesheet
    └── app.js               # Client-side JavaScript (WebSocket protocol, UI logic)
```

Each of the three `web/` files is embedded into the binary at build time as
a C++ raw string (via `embed_web_asset()` in `src/apps/EmbedAsset.cmake`)
and served from its own route (`/`, `/style.css`, `/app.js`) — the browser
loads them the normal way via `<link>`/`<script src>`, but the binary
remains fully self-contained with no external files needed at runtime.

## Usage

```bash
# Build
cmake --build --preset debug-san

# Run with defaults (domain 0, port 8080)
./build/debug-san/bin/OmniscopeDds

# Run with custom DDS domain and HTTP port
./build/debug-san/bin/OmniscopeDds 1 9090

# Open in browser
xdg-open http://localhost:8080
```

From the browser UI you can:

1. **Subscribe** to any dynamically discovered topic to see live messages.
2. **Record** traffic to a `.dat` file.
3. **Upload** a previous recording and **play it back** — this
   re-publishes each message onto the DDS domain with original timing,
   using the exact captured wire bytes.
4. **Stop** playback at any time.

Press **Ctrl+C** in the terminal to shut down cleanly.
