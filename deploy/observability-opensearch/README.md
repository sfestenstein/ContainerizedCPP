# RadarDDSDemo Observability Stack — OpenSearch variant

Same telemetry as [../observability](../observability) — interface
messages/bytes sent & received (per interface name/type/topic) from the DDS
demo (`RadarDDSWorkstation`/`RadarDDSRadar`) *and* the gRPC demo
(`src/apps/GrpcChatLogger`), plus live application logs from all of them —
this variant visualizes it in **OpenSearch Dashboards** instead of Grafana.

The demo apps run as containers alongside the stack, so the whole thing
comes up with one `docker compose up` — see
[../observability/README.md](../observability/README.md) for why this
matters if you're running things inside a devcontainer.

```
--OTLP/gRPC--> data-prepper --route by event type-->
RadarDDSRadar/Workstation --OTLP/gRPC-->
GrpcChatLogger{Server,Reporter,Reader} --OTLP/gRPC--> otel-collector                                         opensearch --> opensearch-dashboards
                        (container: otel-collector:4317)                                                                     (localhost:5601)
```

## Quick Start

```bash
# From the repository root:

# 1. Build the dev base image (one-time — takes a while, fully cached after)
docker build -t containerizedcpp-dev .

# 2. Start everything: both demo app families, the collector, Data Prepper,
#    OpenSearch, OpenSearch Dashboards
docker compose -f deploy/observability-opensearch/docker-compose.yml up --build

# 3. Open the pre-built demo dashboard directly:
http://localhost:5601/app/dashboards#/view/containerizedcpp-demo-dashboard
```

**The dashboard is auto-provisioned** — a one-shot `dashboards-init`
service imports index patterns, visualizations, a live-logs view, and the
combined **"ContainerizedCPP Demo"** dashboard via the Saved Objects API on
every `up` (idempotent, `overwrite=true`), so there's nothing to configure
by hand before demoing. Give it 30–60s after `up` for OpenSearch Dashboards
to report healthy and the import to finish — check with
`docker compose -f deploy/observability-opensearch/docker-compose.yml logs dashboards-init`.

Open **http://localhost:5601 → menu → Dashboards → ContainerizedCPP Demo**
(or the direct link above) and it's ready: four live panels comparing DDS
and gRPC traffic side by side, refreshing every 5s.

## Stopping

```bash
docker compose -f deploy/observability-opensearch/docker-compose.yml down
```

## The demo dashboard

`opensearch-dashboards/demo-dashboard.ndjson` (Saved Objects export,
re-exported after design changes — see "Regenerating the dashboard" below)
provisions four panels on the **ContainerizedCPP Demo** dashboard:

| Panel | Shows |
|-------|-------|
| **Message Rate by Interface** | Live msgs/sec per publisher/client (TSVB `positive_rate`, all `interface.name` terms) — the headline panel putting every DDS and gRPC interface on one live-updating chart. |
| **gRPC Client Call Latency (ms)** | Native gRPC OpenTelemetry plugin data (`grpc.client.attempt.duration`, registered via `grpc::OpenTelemetryPluginBuilder` — zero manual instrumentation), split by RPC method. |
| **Total Messages: DDS vs gRPC** | Running totals bar chart, `interface.type` GRPC vs DDS. |
| **Live Application Logs** | A Discover saved search across every service's `GPINFO`/`GPWARN` stream. |

All panels query `ss4o_metrics-*`/`ss4o_logs-*` directly — no Prometheus-style
metric-name mangling; OTel's dot-separated names (`interface.messages.sent`,
`grpc.client.attempt.duration`, ...) are used as-is.

### Regenerating the dashboard

If you change/add panels in the Dashboards UI, re-export and overwrite the
checked-in file so the next `docker compose up` picks it up:

```bash
curl -s -X POST "http://localhost:5601/api/saved_objects/_export" \
  -H "Content-Type: application/json" -H "osd-xsrf: true" \
  -d '{"objects":[{"type":"dashboard","id":"containerizedcpp-demo-dashboard"},{"type":"index-pattern","id":"ss4o-metrics-pattern"},{"type":"index-pattern","id":"ss4o-logs-pattern"}],"includeReferencesDeep":true}' \
  -o deploy/observability-opensearch/opensearch-dashboards/demo-dashboard.ndjson
```

Both index patterns must be listed explicitly — TSVB visualizations
reference their index pattern as a plain string (`ss4o_metrics-*`) rather
than a formal saved-object reference, so `includeReferencesDeep` alone
won't pull them in. Also note: index patterns created via the API need a
populated `fields` attribute (fetch via
`/api/index_patterns/_fields_for_wildcard?pattern=<pattern>`) — one created
with just `title`/`timeFieldName` causes TSVB's preview endpoint to fail
with a generic `"undefined" is not valid JSON` error that has nothing to
do with the actual TSVB config.

## How it fits together

| File | Purpose |
|------|---------|
| `docker-compose.yml` | Defines the four infra services (collector, Data Prepper, OpenSearch, OpenSearch Dashboards), `dashboards-init`, the two DDS demo apps, and the four gRPC demo apps, plus networking |
| `otel-collector-config.yaml` | Receives OTLP/gRPC on `:4317`; forwards both metrics and logs to Data Prepper over OTLP/gRPC |
| `data-prepper/pipelines.yaml` | Receives OTLP on Data Prepper's unified `otlp` source; routes `LOG` vs `METRIC` events to separate OpenSearch indices |
| `data-prepper/data-prepper-config.yaml` | Disables TLS on Data Prepper's own admin/health server for local-demo simplicity |
| `opensearch-dashboards/demo-dashboard.ndjson` | Saved Objects export (index patterns, visualizations, saved search, dashboard) imported by `dashboards-init` on every startup |

The `radar-radar`/`radar-workstation` services reuse
[deploy/Dockerfile](../Dockerfile) and [deploy/cyclonedds.xml](../cyclonedds.xml)
— exactly as in [../observability/docker-compose.yml](../observability/docker-compose.yml)
— with `OTEL_EXPORTER_OTLP_ENDPOINT=http://otel-collector:4317` redirecting
their OTLP export to the collector service on this compose network. The
`chatlogger-server`/`chatlogger-reporter`/`chatlogger-reader-a`/
`chatlogger-reader-b` services build from the same
[deploy/Dockerfile](../Dockerfile) image (it builds both app families) and
use the same env var redirect; the reporter/readers are pointed at
`chatlogger-server:50051` (the server's default `0.0.0.0:50051` listen
address, reached by its compose service name) instead of gRPC's
`localhost:50051` default. No application or `src/libs/Observability` code
changes are needed to swap observability backends — the apps only ever
speak generic OTLP/gRPC.

**Why Data Prepper and not a direct OpenSearch exporter on the
collector?** The OpenTelemetry Collector Contrib `opensearchexporter`
currently only supports logs and traces (alpha stability) — not metrics.
Since this demo's whole point is the interface message/byte counters,
Data Prepper's `otel_metrics`-aware `otlp` source is the supported path
for getting OTel metrics into OpenSearch, using the same
SS4O ("Simple Schema for Observability") index conventions OpenSearch's
own Observability dashboards expect.

**Security note**: both the OpenSearch security plugin and OpenSearch
Dashboards security plugin are disabled
(`DISABLE_SECURITY_PLUGIN`/`DISABLE_SECURITY_DASHBOARDS_PLUGIN`), and TLS
is disabled between the collector, Data Prepper, and OpenSearch. This is a
local-demo-only convenience choice — mirroring the Grafana stack's
anonymous-auth setup — and is not suitable for any shared or
internet-reachable deployment.
