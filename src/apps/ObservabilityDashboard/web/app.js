// Observability Dashboard client.
//
// Connects to the dashboard's own WebSocket (/ws) and renders whatever
// snapshot arrives. The backend always sends the full current snapshot
// (OTel's cumulative aggregation temporality means every export already
// carries running totals), so the client simply renders the latest values
// for the four traffic counters.

const state = {
   // serviceName -> { cardEl }
   cards: new Map(),
};

function $(selector, root = document) {
   return root.querySelector(selector);
}

function setStatus(connected) {
   $('#statusDot').classList.toggle('connected', connected);
   $('#statusText').textContent = connected ? 'Connected' : 'Disconnected';
   $('#footerInfo').textContent = connected
      ? 'Live -- receiving OTLP/HTTP metric exports'
      : 'Disconnected -- retrying...';
}

function formatBytes(bytes) {
   if (bytes >= 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
   if (bytes >= 1024) return (bytes / 1024).toFixed(1) + ' KB';
   return Math.round(bytes) + ' B';
}

function formatCount(value) {
   return Math.round(value).toLocaleString();
}

function formatBytesPerSec(value) {
   return formatBytes(value) + '/s';
}

function formatPercent(value) {
   return value.toFixed(1) + '%';
}

function sumValues(points) {
   return (points || []).reduce((total, p) => total + p.value, 0);
}

function averageValue(points) {
   if (!points || points.length === 0) return 0;
   return sumValues(points) / points.length;
}

// --- Card lifecycle ----------------------------------------------------

function getOrCreateCard(serviceName) {
   let entry = state.cards.get(serviceName);
   if (entry) return entry;

   $('#emptyState')?.remove();

   const template = document.getElementById('serviceCardTemplate');
   const fragment = template.content.cloneNode(true);
   const cardEl = fragment.querySelector('.card');
   cardEl.querySelector('.service-name').textContent = serviceName;

   document.getElementById('services').appendChild(fragment);

   entry = { cardEl, latencyHistory: [] };
   state.cards.set(serviceName, entry);
   return entry;
}

// --- Rendering -----------------------------------------------------------

function renderService(serviceName, snapshot) {
   const { cardEl } = getOrCreateCard(serviceName);
   const metrics = snapshot.metrics || {};

   const updatedDate = new Date(snapshot.updatedAtMs);
   cardEl.querySelector('.updated-at').textContent =
      'updated ' + updatedDate.toLocaleTimeString();

   cardEl.querySelector('.messages-sent').textContent =
      formatCount(sumValues(metrics['interface.messages.sent']));
   cardEl.querySelector('.messages-received').textContent =
      formatCount(sumValues(metrics['interface.messages.received']));
   cardEl.querySelector('.bytes-sent').textContent =
      formatBytes(sumValues(metrics['interface.bytes.sent']));
   cardEl.querySelector('.bytes-received').textContent =
      formatBytes(sumValues(metrics['interface.bytes.received']));

   // Throughput histograms — backend reduces each histogram to mean (sum/count)
   const hasThroughput =
      metrics['interface.bytes.sent.per_call'] ||
      metrics['interface.bytes.received.per_call'] ||
      metrics['interface.bytes.sent.per_second'] ||
      metrics['interface.bytes.received.per_second'];
   const throughputSection = cardEl.querySelector('.throughput-section');
   throughputSection.style.display = hasThroughput ? '' : 'none';
   if (hasThroughput) {
      cardEl.querySelector('.avg-bytes-sent-per-call').textContent =
         formatBytes(averageValue(metrics['interface.bytes.sent.per_call']));
      cardEl.querySelector('.avg-bytes-received-per-call').textContent =
         formatBytes(averageValue(metrics['interface.bytes.received.per_call']));
      cardEl.querySelector('.avg-bytes-sent-per-sec').textContent =
         formatBytesPerSec(averageValue(metrics['interface.bytes.sent.per_second']));
      cardEl.querySelector('.avg-bytes-received-per-sec').textContent =
         formatBytesPerSec(averageValue(metrics['interface.bytes.received.per_second']));
   }

   // Process gauges
   const hasProcess =
      metrics['process.cpu.utilization.pct'] ||
      metrics['process.memory.rss.bytes'] ||
      metrics['process.memory.vms.bytes'] ||
      metrics['process.thread.count'];
   const processSection = cardEl.querySelector('.process-section');
   processSection.style.display = hasProcess ? '' : 'none';
   if (hasProcess) {
      cardEl.querySelector('.process-cpu').textContent =
         formatPercent(averageValue(metrics['process.cpu.utilization.pct']));
      cardEl.querySelector('.process-rss').textContent =
         formatBytes(averageValue(metrics['process.memory.rss.bytes']));
      cardEl.querySelector('.process-vms').textContent =
         formatBytes(averageValue(metrics['process.memory.vms.bytes']));
      cardEl.querySelector('.process-threads').textContent =
         formatCount(averageValue(metrics['process.thread.count']));
   }

   // GPU gauges (sysfs: AMD/Intel; absent when no GPU is detected)
   const hasGpu =
      metrics['gpu.utilization.pct'] ||
      metrics['gpu.memory.used.bytes'] ||
      metrics['gpu.memory.total.bytes'];
   const gpuSection = cardEl.querySelector('.gpu-section');
   gpuSection.style.display = hasGpu ? '' : 'none';
   if (hasGpu) {
      cardEl.querySelector('.gpu-utilization').textContent =
         formatPercent(averageValue(metrics['gpu.utilization.pct']));
      cardEl.querySelector('.gpu-memory-used').textContent =
         formatBytes(averageValue(metrics['gpu.memory.used.bytes']));
      cardEl.querySelector('.gpu-memory-total').textContent =
         formatBytes(averageValue(metrics['gpu.memory.total.bytes']));
   }
}

function renderSnapshot(message) {
   const services = message.services || {};
   Object.keys(services)
      .sort()
      .forEach((serviceName) => renderService(serviceName, services[serviceName]));
}

// --- WebSocket connection --------------------------------------------------

function connect() {
   const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
   const ws = new WebSocket(`${protocol}//${window.location.host}/ws`);

   ws.onopen = () => setStatus(true);
   ws.onclose = () => {
      setStatus(false);
      setTimeout(connect, 1500);
   };
   ws.onerror = () => ws.close();
   ws.onmessage = (event) => {
      try {
         const message = JSON.parse(event.data);
         if (message.type === 'snapshot') renderSnapshot(message);
      } catch (err) {
         console.error('Failed to parse dashboard message', err);
      }
   };
}

connect();
