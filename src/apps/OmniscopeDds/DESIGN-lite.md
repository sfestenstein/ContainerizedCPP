# OmniscopeDds — Design at a Glance

A web-based DDS traffic inspector: monitor, record, and replay messages on
a Cyclone DDS domain from a browser, with fully dynamic topic discovery —
no compiled-in IDL types or fixed topic list required.

> Full details, sequence diagrams, and the WebSocket/HTTP protocol
> reference live in [DESIGN.md](DESIGN.md). This doc is the 2-minute
> version.

## What it does

1. **Discover** — watches the DDS domain and lists every active topic as
   publishers come and go.
2. **Monitor** — subscribe to any topic and see live messages in the
   browser, decoded only as far as raw CDR bytes + metadata (no compiled
   type needed).
3. **Record** — capture subscribed topics to a `.dat` (JSON-Lines) file.
4. **Replay** — upload a recording and re-publish it onto the DDS domain
   with original timing, byte-for-byte.

## Shape of the code

```
Browser ⇄ OmniscopeApp (Crow HTTP/WS) ⇄ ITransport ⇄ TransportDds ⇄ DdsCore ⇄ CycloneDDS
                │
                └─ PlaybackEngine (record/replay timing, transport-agnostic)
```

Two deliberate seams, one nested inside the other:

- **`ITransport`** (`ITransport.h`) — the top-level abstraction.
  `OmniscopeApp` and `PlaybackEngine` only ever talk to this interface;
  swapping to a different pub/sub middleware entirely means writing a new
  `ITransport` implementation, no changes above it.
- **`DdsCore`** (`src/libs/DdsCore/`) — a narrower seam *inside*
  `TransportDds`, for the common case of staying on DDS but swapping
  *vendor* (e.g. Cyclone → RTI). Three interfaces
  (`IDiscoveryService`/`IRawTopicSubscriber`/`IRawTopicPublisher`) carry
  topic name, type name, QoS, and raw CDR bytes — no vendor SDK dependency.
  `TransportDds` composes vendor-specific implementations of these
  (currently `Cyclone*` classes in `src/libs/CycloneDDS/`) and does nothing
  vendor-specific itself.

The "any type, no compiled IDL" trick is `BlobSertype`
(`src/libs/CycloneDDS/BlobSertype.cpp`) — a hand-rolled CycloneDDS sertype
that carries raw, undecoded CDR bytes keyed only by a type-name string.

## Key files

| File | Role |
|---|---|
| `main.cpp` | Entry point, argument parsing |
| `OmniscopeApp.h/.cpp` | HTTP/WebSocket orchestrator, owns transports + recording state |
| `ITransport.h` | The one interface the app layer depends on |
| `TransportDds.h/.cpp` | Thin `ITransport` adapter composing `DdsCore` pieces |
| `RawSampleJsonCodec.h/.cpp` | `DdsCore::RawSample` ⇄ wire-JSON, zero DDS dependency |
| `PlaybackEngine.h/.cpp` | Recording playback with original timing, transport-agnostic |
| `src/libs/DdsCore/` | Vendor-agnostic interfaces (no DDS SDK dependency) |
| `src/libs/CycloneDDS/Cyclone*` | CycloneDDS implementations of the `DdsCore` interfaces |

## Non-obvious things worth knowing

- **Recording only captures subscribed topics.** A topic visible in the
  list but never clicked/subscribed produces no data in the recording,
  even though it's discoverable.
- **Playback never gates on live discovery.** It publishes to every
  registered transport unconditionally — it must work even when the
  original live publisher (or this process's own knowledge of it) is long
  gone. See DESIGN.md's Playback section for why this matters.
- **`DDSSubscriber<T>`/`DDSPublisher<T>`** (also in `src/libs/CycloneDDS/`)
  are unrelated — compile-time-typed templates used by `RadarDDSDemo`, not
  by this app.
