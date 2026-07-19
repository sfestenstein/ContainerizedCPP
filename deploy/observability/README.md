# RadarDDSDemo Observability Stack

Watch interface traffic metrics (messages/bytes sent & received, per
interface name/type/topic) and live application logs from
`RadarDDSWorkstation` and `RadarDDSRadar`, together, in Grafana.

The demo apps run as containers alongside the observability stack, so the
whole thing comes up with one `docker compose up` — no dependency on the host
machine's networking to reach the collector (e.g. running the apps inside a
devcontainer while the stack runs separately on the host doesn't work:
they're on unconnected Docker networks, and the apps' `localhost:4317`
doesn't resolve to the collector).

```
                                          --scrape--> prometheus --\
RadarDDSRadar/Workstation --OTLP/gRPC--> otel-collector             --> grafana
     (container: otel-collector:4317)    --OTLP/HTTP-> loki       --/  (localhost:3000)
```

## Quick Start

```bash
# From the repository root:

# 1. Build the dev base image (one-time — takes a while, fully cached after)
docker build -t containerizedcpp-dev .

# 2. Start everything: the two demo apps, the collector, Prometheus, Grafana
docker compose -f deploy/observability/docker-compose.yml up --build

# 3. Open the dashboard — no login required
open http://localhost:3000   # or xdg-open on Linux
```

The **RadarDDSDemo - DDS Message Traffic** dashboard loads automatically and
shows live per-topic send/receive rates plus running totals.

For live terminal narration alongside the dashboard during a demo:

```bash
docker compose -f deploy/observability/docker-compose.yml logs -f radar-radar radar-workstation
```

Prometheus's own graph UI is also available at `http://localhost:9090/graph`
if you want to run ad-hoc PromQL queries.

## Stopping

```bash
docker compose -f deploy/observability/docker-compose.yml down
```

## Running the apps locally instead

If you'd rather run the apps directly on the host (e.g. for local
development outside a devcontainer), you can still start just the
observability services and point the locally-run apps at
`localhost:4317`, which the stack publishes:

```bash
docker compose -f deploy/observability/docker-compose.yml up -d otel-collector prometheus grafana
./build/<preset>/bin/RadarDDSRadar 0
./build/<preset>/bin/RadarDDSWorkstation 0
```

This works because both apps default to `localhost:4317` for OTLP export
when `OTEL_EXPORTER_OTLP_ENDPOINT` isn't set — only relevant if "localhost"
from the apps' point of view actually reaches the collector container (true
when running the apps directly on the same host as Docker; not true from
inside a separate devcontainer).

## How it fits together

| File | Purpose |
|------|---------|
| `docker-compose.yml` | Defines all six services (collector, Prometheus, Loki, Grafana, and the two demo apps) and networking |
| `otel-collector-config.yaml` | Receives OTLP/gRPC on `:4317`; re-exposes metrics for Prometheus to scrape, forwards logs to Loki over OTLP/HTTP |
| `prometheus.yml` | Scrapes the collector every 5s |
| `grafana/provisioning/datasources/` | Auto-wires Prometheus and Loki as Grafana datasources on startup |
| `grafana/provisioning/dashboards/` | Tells Grafana to auto-load dashboards from `grafana/dashboards/` |
| `grafana/dashboards/radar-dds-demo.json` | The dashboard itself — metrics panels plus a live logs panel |

The `radar-radar`/`radar-workstation` services reuse
[deploy/Dockerfile](../Dockerfile) and [deploy/cyclonedds.xml](../cyclonedds.xml)
— the same multi-stage build and DDS peer-discovery config
[deploy/docker-compose.yml](../docker-compose.yml) uses — with one addition:
`OTEL_EXPORTER_OTLP_ENDPOINT=http://otel-collector:4317` redirects their OTLP
export from the default `localhost:4317` to the collector service on this
compose network.

**Metric naming note**: the Prometheus exporter renames OTel metrics —
dots become underscores and counters get a `_total` suffix. So
`interface.messages.sent` (an OTel counter, see
[src/libs/Observability](../../src/libs/Observability)) becomes
`interface_messages_sent_total` in PromQL, with the `interface_name`,
`interface_type`, and `topic` attributes preserved as labels.

**Logs**: `CommonUtils::GeneralLogger` gets a third sink
(`Observability::createOtelLogSink()`, attached via `GeneralLogger::addSink()`)
alongside its existing console/file sinks — every `GPINFO`/`GPWARN`/etc. call
site is unmodified, but now also forwards to the collector over OTLP, which
ships logs on to Loki (natively over `/otlp/v1/logs` — no dedicated exporter
needed) for the dashboard's logs panel to query.
