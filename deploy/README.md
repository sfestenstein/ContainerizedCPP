# StarterCpp Microservice Deployment

A three-service containerized deployment of the StarterCpp DDS system.  
This guide is written for developers familiar with containerized **development** who are
learning containerized **deployment** and microservice architecture.

---

## Quick Start

```bash
# From the repository root:

# 1. Build the dev base image (one-time — takes a while, fully cached after)
docker build -t startercpp-dev .

# 2. Build and launch all three microservices
docker compose -f deploy/docker-compose.yml up --build

# 3. Open Omniscope in your browser
open http://localhost:8080   # or xdg-open on Linux
```

You should see live DDS traffic (SensorReading and TrackUpdate messages) streaming
into the Omniscope web UI at 10 Hz.

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
│  Stage 1: "builder"  (FROM startercpp-dev)      │
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
| `dds-publisher` | `DDSPublisher 0` | Publishes SensorReading + TrackUpdate on DDS domain 0 |
| `dds-subscriber` | `DDSSubscriber 0` | Subscribes to and logs all DDS messages |
| `omniscope` | `Omniscope 0 8080` | Web-based traffic inspector (HTTP + WebSocket) |

Why one process per container?
- **Independent scaling** — run 3 publishers and 1 subscriber if needed
- **Independent failure** — a crashed subscriber doesn't take down the publisher
- **Independent updates** — redeploy one service without touching others
- **Simple logging** — container stdout IS the service log

### Docker Networking: How Containers Find Each Other

The `docker-compose.yml` creates a **user-defined bridge network** called `dds-net`.
This gives us:

1. **DNS by container name** — `dds-publisher` resolves to that container's IP
2. **Network isolation** — only containers on `dds-net` can talk to each other
3. **Multicast support** — required for DDS automatic discovery

The `cyclonedds.xml` file configures DDS peer discovery using both multicast
(works on bridge) and explicit peer hostnames (works everywhere):

```xml
<Peers>
   <Peer address="dds-publisher" />
   <Peer address="dds-subscriber" />
   <Peer address="omniscope" />
</Peers>
```

Docker's DNS resolves these names to container IPs on the `dds-net` network.

### Port Mapping: Reaching Containers from Outside

Only Omniscope needs to be accessible from your browser. The compose file maps:

```yaml
ports:
  - "8080:8080"   # HOST_PORT:CONTAINER_PORT
```

This means: "Forward traffic arriving at the **host's** port 8080 into the
**container's** port 8080." The publisher and subscriber don't expose any ports
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
│  ┌──────────────┐    DDS     ┌──┴───────────────┐           │
│  │ dds-publisher │──────────▶│  omniscope        │           │
│  │ SensorTopic   │           │  :8080 (HTTP/WS)  │           │
│  │ TrackTopic    │           └──────────────────┘           │
│  │ @ 10 Hz       │    DDS     ┌──────────────────┐          │
│  └──────────────┘──────────▶│  dds-subscriber    │          │
│                              │  (logs messages)   │          │
│                              └──────────────────┘           │
└─────────────────────────────────────────────────────────────┘
```

---

## Useful Commands

```bash
# View live logs from all services (Ctrl+C to stop watching)
docker compose -f deploy/docker-compose.yml logs -f

# View logs from one specific service
docker compose -f deploy/docker-compose.yml logs -f omniscope

# Stop all services (containers are removed)
docker compose -f deploy/docker-compose.yml down

# Rebuild after code changes (only the cmake build re-runs — deps are cached)
docker compose -f deploy/docker-compose.yml up --build

# Scale the publisher (run 3 instances)
docker compose -f deploy/docker-compose.yml up --build --scale dds-publisher=3

# Open a shell inside a running container for debugging
docker exec -it omniscope bash

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
to Kubernetes Service DNS names (`dds-publisher.default.svc.cluster.local`).
