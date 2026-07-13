# ContainerizedCPP Microservice Deployment

A three-service containerized deployment of the ContainerizedCPP DDS system.  
This guide is written for developers familiar with containerized **development** who are
learning containerized **deployment** and microservice architecture.

---

## Quick Start

```bash
# From the repository root:

# 1. Build the dev base image (one-time — takes a while, fully cached after)
docker build -t containerizedcpp-dev .

# 2. Build and launch all three microservices
docker compose -f deploy/docker-compose.yml up --build

# 3. Open OmniscopeDds in your browser
open http://localhost:8080   # or xdg-open on Linux
```

You should see the radar's five topics (`RadarCommand`, `RadarCommandStatus`,
`RadarTrack`, `RadarComponentStatus`, `RadarAlert`) appear dynamically in the
OmniscopeDds web UI as `radar-radar` and `radar-workstation` exchange messages.

---

## Key Concepts for Microservice Deployment

### Dev Container vs. Deploy Container

You already use a **dev container** — a fat image with compilers, debuggers, source
code mounted as a volume.  A **deploy container** is the opposite:

| | Dev Container | Deploy Container |
|---|---|---|
| **Purpose** | Write & debug code | Run in production |
| **Base image** | Full Ubuntu + toolchain | Minimal Ubuntu runtime |
| **Source code** | Mounted as a volume | Not present — only compiled binaries |
| **Size** | Large (~2+ GB) | Small (~100-200 MB) |
| **Lifetime** | Long-lived, interactive | Ephemeral, restartable |

### Multi-Stage Docker Builds

The [deploy/Dockerfile](Dockerfile) uses a **multi-stage build** — the key technique
that separates build-time from run-time:

```
┌─────────────────────────────────────────────────┐
│  Stage 1: "builder"  (FROM containerizedcpp-dev)      │
│  • Has all compilers, headers, build tools      │
│  • COPY source → cmake configure → cmake build  │
│  • Produces binaries in /workspace/build/...    │
│  • This stage is DISCARDED in the final image   │
└────────────────────┬────────────────────────────┘
                     │ COPY --from=builder
┌────────────────────▼────────────────────────────┐
│  Stage 2: "runtime"  (FROM ubuntu:24.04)        │
│  • Minimal OS — no compiler, no source code     │
│  • Only shared libraries + application binaries │
│  • This is what actually runs in production     │
└─────────────────────────────────────────────────┘
```

The `COPY --from=builder` instruction is how you cherry-pick artifacts from the
build stage without bringing along the entire toolchain.

### One Process Per Container (The Microservice Pattern)

Each service runs **exactly one process**:

| Service | Binary | What It Does |
|---------|--------|-------------|
| `radar-radar` | `RadarDDSRadar 0` | Publishes RadarTrack (Best Effort), ComponentStatus + RadarAlert (TransientLocal); replies to Command |
| `radar-workstation` | `RadarDDSWorkstation 0` | Sends Command; subscribes to and logs the practical effect of each topic's QoS profile |
| `omniscope-dds` | `OmniscopeDds 0 8080` | Web-based DDS traffic inspector (HTTP + WebSocket) — dynamic topic discovery, recording, and playback, no fixed topic list |

Why one process per container?
- **Independent scaling** — run multiple radar nodes if needed
- **Independent failure** — a crashed workstation doesn't take down the radar
- **Independent updates** — redeploy one service without touching others
- **Simple logging** — container stdout IS the service log

### Docker Networking: How Containers Find Each Other

The `docker-compose.yml` creates a **user-defined bridge network** called `dds-net`.
This gives us:

1. **DNS by container name** — `radar-radar` resolves to that container's IP
2. **Network isolation** — only containers on `dds-net` can talk to each other
3. **Multicast support** — required for DDS automatic discovery

The `cyclonedds.xml` file configures DDS peer discovery using both multicast
(works on bridge) and explicit peer hostnames (works everywhere):

```xml
<Peers>
   <Peer address="radar-radar" />
   <Peer address="radar-workstation" />
   <Peer address="omniscope-dds" />
</Peers>
```

Docker's DNS resolves these names to container IPs on the `dds-net` network.

### Port Mapping: Reaching Containers from Outside

Only OmniscopeDds needs to be accessible from your browser. The compose file maps:

```yaml
ports:
  - "8080:8080"   # HOST_PORT:CONTAINER_PORT
```

This means: "Forward traffic arriving at the **host's** port 8080 into the
**container's** port 8080." The radar and workstation don't expose any ports
because they only communicate internally via DDS.

---

## Architecture

```
                              Your Machine
                    ┌──────────────────────────────┐
                    │  Browser → localhost:8080     │
                    └─────────────┬────────────────┘
                                  │ port mapping
┌─────────────────────────────────┼───────────────────────────┐
│  Docker bridge network          │          (dds-net)         │
│                                 │                           │
│  ┌──────────────────┐    DDS  ┌──┴───────────────┐          │
│  │ radar-radar       │───────▶│  omniscope-dds    │          │
│  │ RadarTrack        │        │  :8080 (HTTP/WS)  │          │
│  │ ComponentStatus   │        └──────────────────┘          │
│  │ RadarAlert        │  DDS     ┌──────────────────────┐    │
│  └──────────────────┘─────────▶│  radar-workstation    │    │
│           ▲                    │  (sends Command,      │    │
│           └────────────────────│   logs QoS effects)   │    │
│                    Command     └──────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## Useful Commands

```bash
# View live logs from all services (Ctrl+C to stop watching)
docker compose -f deploy/docker-compose.yml logs -f

# View logs from one specific service
docker compose -f deploy/docker-compose.yml logs -f omniscope-dds

# Stop all services (containers are removed)
docker compose -f deploy/docker-compose.yml down

# Rebuild after code changes (only the cmake build re-runs — deps are cached)
docker compose -f deploy/docker-compose.yml up --build

# Scale the radar (run 3 instances)
docker compose -f deploy/docker-compose.yml up --build --scale radar-radar=3

# Open a shell inside a running container for debugging
docker exec -it omniscope-dds bash

# See container resource usage
docker stats
```

---

## File Reference

| File | Purpose |
|------|---------|
| `deploy/Dockerfile` | Multi-stage build: compiles code → produces slim runtime image |
| `deploy/docker-compose.yml` | Defines the three services, networking, and port mapping |
| `deploy/cyclonedds.xml` | DDS peer discovery config (multicast + unicast hostnames) |

---

## Next Steps: Path to Kubernetes

Once comfortable with Compose, the next steps toward production-grade orchestration:

1. **Docker Compose (you are here)** — single-host, great for development & learning
2. **Push images to a registry** — `docker tag` / `docker push` to Docker Hub or GHCR
3. **Kind or Minikube** — local single-node Kubernetes cluster
4. **Kubernetes manifests** — Deployments, Services, ConfigMaps replace compose services
5. **Helm charts** — templated K8s manifests for parameterized deployments

The `cyclonedds.xml` unicast peer list already uses DNS hostnames, which map directly
to Kubernetes Service DNS names (`radar-radar.default.svc.cluster.local`).
