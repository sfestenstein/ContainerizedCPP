// Observability Dashboard client.
//
// Connects to the dashboard's own WebSocket (/ws) and renders whatever
// snapshot arrives. The backend always sends the full current snapshot
// (OTel's cumulative aggregation temporality means every export already
// carries running totals) -- this script only keeps its own short rolling
// history for the latency sparkline, everything else is a direct render of
// the latest message.

const MAX_SPARKLINE_POINTS = 40;

const state = {
   // serviceName -> { cardEl, latencyHistory: number[] }
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

function sumValues(points) {
   return (points || []).reduce((total, p) => total + p.value, 0);
}

function averageValue(points) {
   if (!points || points.length === 0) return 0;
   return sumValues(points) / points.length;
}

function attrKey(point, name) {
   return point.attributes && point.attributes[name] !== undefined ? point.attributes[name] : '';
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

function drawSparkline(canvas, values) {
   const ctx = canvas.getContext('2d');
   const w = canvas.width;
   const h = canvas.height;
   ctx.clearRect(0, 0, w, h);

   if (values.length < 2) return;

   const max = Math.max(...values, 0.001);
   const min = Math.min(...values, 0);
   const range = max - min || 1;
   const stepX = w / (MAX_SPARKLINE_POINTS - 1);
   const startIndex = MAX_SPARKLINE_POINTS - values.length;

   ctx.strokeStyle = '#4ee8a8';
   ctx.lineWidth = 2;
   ctx.beginPath();
   values.forEach((v, i) => {
      const x = (startIndex + i) * stepX;
      const y = h - ((v - min) / range) * (h - 4) - 2;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
   });
   ctx.stroke();
}

// --- Rendering -----------------------------------------------------------

function renderService(serviceName, snapshot) {
   const { cardEl, latencyHistory } = getOrCreateCard(serviceName);
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

   const latencyPoints = metrics['interface.latency'];
   if (latencyPoints && latencyPoints.length > 0) {
      const avgMs = averageValue(latencyPoints);
      cardEl.querySelector('.latency-value').textContent = avgMs.toFixed(2) + ' ms';
      latencyHistory.push(avgMs);
      if (latencyHistory.length > MAX_SPARKLINE_POINTS) latencyHistory.shift();
      drawSparkline(cardEl.querySelector('.sparkline'), latencyHistory);
   }

   const cpuPoints = metrics['process.cpu.utilization'];
   if (cpuPoints && cpuPoints.length > 0) {
      const cpuPct = Math.max(0, Math.min(100, averageValue(cpuPoints) * 100));
      const cpuBar = cardEl.querySelector('.cpu-bar');
      cpuBar.style.width = cpuPct.toFixed(1) + '%';
      cpuBar.classList.toggle('warn', cpuPct > 80);
      cardEl.querySelector('.cpu-value').textContent = cpuPct.toFixed(1) + '%';
   }

   const memPoints = metrics['process.memory.usage'];
   if (memPoints && memPoints.length > 0) {
      const memBytes = averageValue(memPoints);
      // Scale the bar against 512MB as a rough "full" reference -- there's
      // no fixed ceiling for RSS, this just keeps the bar visually useful.
      const memPct = Math.max(0, Math.min(100, (memBytes / (512 * 1024 * 1024)) * 100));
      cardEl.querySelector('.mem-bar').style.width = memPct.toFixed(1) + '%';
      cardEl.querySelector('.mem-value').textContent = formatBytes(memBytes);
   }

   const activePoints = metrics['interface.subscriber.active'];
   const topicList = cardEl.querySelector('.topic-list');
   if (activePoints) {
      topicList.innerHTML = '';
      activePoints
         .slice()
         .sort((a, b) => attrKey(a, 'topic').localeCompare(attrKey(b, 'topic')))
         .forEach((point) => {
            const chip = document.createElement('span');
            chip.className = 'topic-chip';
            const dot = document.createElement('span');
            dot.className = 'topic-dot' + (point.value > 0 ? ' active' : '');
            chip.appendChild(dot);
            chip.appendChild(document.createTextNode(attrKey(point, 'topic') || '(unknown)'));
            topicList.appendChild(chip);
         });
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
