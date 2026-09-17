const WS_URL = `ws://${location.host}/ws`;
let ws = null;
let topics = {};          // {name: {subscribed: bool, count: int, qosMismatch: bool}}
let messages = [];
let selectedMsgIdx = -1;
let selectedTopic = null;
let recording = false;
let playing = false;
let recordingLoaded = false;
let recordingLineCount = 0;
let ctxTopicName = null;
const MAX_MESSAGES = 10000;

// --- WebSocket ---
function connect() {
   ws = new WebSocket(WS_URL);
   ws.onopen = () => {
      document.getElementById('statusDot').classList.add('connected');
      document.getElementById('statusText').textContent = 'Connected';
   };
   ws.onclose = () => {
      document.getElementById('statusDot').classList.remove('connected');
      document.getElementById('statusText').textContent = 'Disconnected';
      setTimeout(connect, 2000);
   };
   ws.onmessage = (evt) => {
      const msg = JSON.parse(evt.data);
      if (msg.type === 'topics') {
         handleTopicList(msg.topics);
      } else if (msg.type === 'message') {
         handleIncomingMessage(msg);
      } else if (msg.type === 'recording_started') {
         recording = true;
         updateRecUI();
      } else if (msg.type === 'recording_stopped') {
         recording = false;
         updateRecUI();
         if (msg.filename) {
            document.getElementById('footerInfo').textContent =
               `Recording saved: ${msg.filename}`;
         }
      } else if (msg.type === 'recording_loaded') {
         recordingLoaded = true;
         recordingLineCount = msg.count;
         updatePlaybackUI();
         document.getElementById('footerInfo').textContent =
            `Recording loaded: ${msg.count} messages`;
      } else if (msg.type === 'playback_started') {
         playing = true;
         updatePlaybackUI();
         document.getElementById('footerInfo').textContent =
            `Playback in progress: ${msg.count} messages...`;
      } else if (msg.type === 'playback_progress') {
         updateProgress(msg.current, msg.total);
      } else if (msg.type === 'playback_finished') {
         playing = false;
         updatePlaybackUI();
         resetProgress();
         document.getElementById('footerInfo').textContent = 'Playback finished';
      } else if (msg.type === 'playback_stopped') {
         playing = false;
         updatePlaybackUI();
         resetProgress();
         document.getElementById('footerInfo').textContent = 'Playback stopped';
      }
   };
}

function send(obj) {
   if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(obj));
   }
}

// --- Topics ---
function handleTopicList(topicArr) {
   for (const t of topicArr) {
      if (!topics[t.name]) {
         topics[t.name] = {
            subscribed: t.subscribed,
            transport: t.transport,
            count: 0,
            publisherCount: t.publisher_count || 0,
            subscriberCount: t.subscriber_count || 0,
            publisherApps: Array.isArray(t.publisher_apps) ? t.publisher_apps : [],
            subscriberApps: Array.isArray(t.subscriber_apps) ? t.subscriber_apps : [],
            publisherQosProfiles: Array.isArray(t.publisher_qos_profiles) ? t.publisher_qos_profiles : [],
            subscriberQosProfiles: Array.isArray(t.subscriber_qos_profiles) ? t.subscriber_qos_profiles : [],
            qosMismatch: !!t.qos_mismatch,
            qosMismatchReason: t.qos_mismatch_reason || ''
         };
      } else {
         topics[t.name].subscribed = t.subscribed;
         topics[t.name].transport = t.transport;
         topics[t.name].publisherCount = t.publisher_count || 0;
         topics[t.name].subscriberCount = t.subscriber_count || 0;
         topics[t.name].publisherApps = Array.isArray(t.publisher_apps) ? t.publisher_apps : [];
         topics[t.name].subscriberApps = Array.isArray(t.subscriber_apps) ? t.subscriber_apps : [];
         topics[t.name].publisherQosProfiles = Array.isArray(t.publisher_qos_profiles) ? t.publisher_qos_profiles : [];
         topics[t.name].subscriberQosProfiles = Array.isArray(t.subscriber_qos_profiles) ? t.subscriber_qos_profiles : [];
         topics[t.name].qosMismatch = !!t.qos_mismatch;
         topics[t.name].qosMismatchReason = t.qos_mismatch_reason || '';
      }
   }
   renderTopologyGraph();
   renderTopics();
   if (selectedTopic && topics[selectedTopic])
      renderTopicDetail(selectedTopic);
}

function renderTopologyGraph() {
   const host = document.getElementById('topologyGraph');
   if (!host) return;

   const items = Object.entries(topics).sort(([a], [b]) => a.localeCompare(b));
   if (!items.length) {
      host.innerHTML = '<div class="topology-empty">No DDS topics discovered yet.</div>';
      return;
   }

   const width = Math.max(host.clientWidth || 0, 220);
   const rootY = 34;
   const topicYStart = 98;
   const topicHeight = 58;
   const rowGap = 28;
   const columns = width < 380 ? 1 : width < 620 ? 2 : 3;
   const columnWidth = Math.max(170, Math.floor(width / columns));
   const rows = Math.ceil(items.length / columns);
   const height = topicYStart + rows * (topicHeight + rowGap) + 36;
   const rootX = Math.floor(width / 2);

   const nodes = [];
   const lines = [];

   nodes.push(`
      <g class="topology-node root" transform="translate(${rootX - 60}, 10)">
         <rect rx="14" ry="14" width="120" height="34"></rect>
         <text x="60" y="22" text-anchor="middle" class="topology-node-label">DDS</text>
      </g>`);

   items.forEach(([name, info], index) => {
      const col = index % columns;
      const row = Math.floor(index / columns);
      const x = Math.floor(col * columnWidth + columnWidth / 2);
      const y = topicYStart + row * (topicHeight + rowGap);
      const topicW = Math.min(170, columnWidth - 14);
      const topicX = x - Math.floor(topicW / 2);
      const centerX = x;
      const mismatch = !!info.qosMismatch;

      lines.push(`<line class="topology-line ${mismatch ? 'mismatch' : ''}" x1="${rootX}" y1="44" x2="${centerX}" y2="${y}"></line>`);

      const selected = selectedTopic === name;
      nodes.push(`
         <g class="topology-node topic ${mismatch ? 'mismatch' : ''} ${selected ? 'selected' : ''}" data-topic="${esc(name)}" transform="translate(${topicX}, ${y})">
            <rect rx="14" ry="14" width="${topicW}" height="44"></rect>
            <text x="${topicW / 2}" y="18" text-anchor="middle" class="topology-node-label">${esc(name)}</text>
            <text x="${topicW / 2}" y="34" text-anchor="middle" class="topology-node-count">P${info.publisherCount || 0} / S${info.subscriberCount || 0}${mismatch ? '  MISMATCH' : ''}</text>
         </g>`);
   });

   host.innerHTML = `
      <svg class="topology-svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" preserveAspectRatio="xMinYMin meet" role="img" aria-label="DDS topology graph">
         ${lines.join('')}
         ${nodes.join('')}
      </svg>`;

   host.querySelectorAll('[data-topic]').forEach(node => {
      node.addEventListener('click', (evt) => {
         evt.stopPropagation();
         const topicName = node.getAttribute('data-topic');
         if (!topicName) return;
         selectedTopic = topicName;
         renderTopics();
         renderTopologyGraph();
         renderTopicDetail(topicName);
      });
   });
}

function renderTopics() {
   const el = document.getElementById('topicsList');
   el.innerHTML = '';

   // Group topics by transport
   const groups = {};
   for (const [name, info] of Object.entries(topics).sort()) {
      const tr = info.transport || 'Unknown';
      if (!groups[tr]) groups[tr] = [];
      groups[tr].push({ name, info });
   }

   for (const [transport, items] of Object.entries(groups).sort()) {
      const tagClass = transport.toLowerCase();
      const group = document.createElement('div');
      group.className = 'transport-group';

      // Section header with colored tag
      const hdr = document.createElement('div');
      hdr.className = 'transport-header';
      hdr.innerHTML = `<span class="transport-tag ${esc(tagClass)}">${esc(transport)}</span>`
                    + ` ${items.length} topic${items.length !== 1 ? 's' : ''}`;
      group.appendChild(hdr);

      // Topic rows
      for (const { name, info } of items) {
         const div = document.createElement('div');
         const isSelected = selectedTopic === name;
         div.className = `topic-item ${info.qosMismatch ? 'qos-mismatch' : ''} ${isSelected ? 'selected' : ''}`;
         div.title = info.qosMismatch ? info.qosMismatchReason : '';
         div.innerHTML = `
            <span class="topic-name ${info.qosMismatch ? 'qos-mismatch' : ''}">${esc(name)}</span>
            <span class="topic-badge ${info.subscribed ? 'subscribed' : ''}">
               ${info.subscribed ? 'SUB' : 'OFF'}
            </span>`;
         div.oncontextmenu = (e) => { e.preventDefault(); showCtxMenu(e, name); };
         const topicNameEl = div.querySelector('.topic-name');
         const topicBadgeEl = div.querySelector('.topic-badge');

         const showTopicDetails = () => {
            selectedTopic = name;
            renderTopics();
            renderTopologyGraph();
            renderTopicDetail(name);
         };

         if (topicNameEl) {
            topicNameEl.style.cursor = 'pointer';
            topicNameEl.onclick = (e) => {
               e.stopPropagation();
               showTopicDetails();
            };
         }

         if (topicBadgeEl) {
            topicBadgeEl.onclick = (e) => {
               e.stopPropagation();
               toggleSubscribe(name);
            };
         }

         div.onclick = () => {
            showTopicDetails();
         };

         div.ondblclick = (e) => {
            if (e.target instanceof Element && e.target.classList.contains('topic-badge')) {
               return;
            }
            toggleSubscribe(name);
         };
         group.appendChild(div);
      }

      el.appendChild(group);
   }
}

function toggleSubscribe(name) {
   const info = topics[name];
   if (!info) return;
   const action = info.subscribed ? 'unsubscribe' : 'subscribe';
   send({ type: action, topic: name });
}

// --- Context menu ---
function showCtxMenu(e, topicName) {
   ctxTopicName = topicName;
   const menu = document.getElementById('ctxMenu');
   const info = topics[topicName];
   document.getElementById('ctxToggleSub').textContent =
      info && info.subscribed ? `Unsubscribe from ${topicName}` : `Subscribe to ${topicName}`;
   menu.style.left = e.clientX + 'px';
   menu.style.top = e.clientY + 'px';
   menu.classList.add('visible');
}

function ctxToggleSubscribe() {
   if (ctxTopicName) toggleSubscribe(ctxTopicName);
   document.getElementById('ctxMenu').classList.remove('visible');
}

document.addEventListener('click', () => {
   document.getElementById('ctxMenu').classList.remove('visible');
});

// --- Messages ---
function handleIncomingMessage(msg) {
   const entry = {
      topic: msg.topic,
      time: msg.timestamp || new Date().toISOString(),
      data: msg.data,
      raw: msg
   };

   messages.push(entry);
   if (messages.length > MAX_MESSAGES) messages.shift();

   if (topics[msg.topic]) topics[msg.topic].count++;

   appendMessageRow(entry, messages.length - 1);
   document.getElementById('msgCount').textContent = messages.length;
   document.getElementById('footerStats').textContent =
      `${messages.length} messages`;
}

function appendMessageRow(entry, idx) {
   const el = document.getElementById('messagesList');
   const div = document.createElement('div');
   div.className = 'msg-item';
   div.dataset.idx = idx;

   const timeStr = new Date(entry.time).toLocaleTimeString('en-US', {
      hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit',
      fractionalSecondDigits: 3
   });

   div.innerHTML = `
      <span class="msg-topic">${esc(entry.topic)}</span>
      <span class="msg-time">${timeStr}</span>`;
   div.onclick = () => selectMessage(idx);
   el.appendChild(div);

   // Auto-scroll to bottom
   el.scrollTop = el.scrollHeight;
}

function selectMessage(idx) {
   selectedMsgIdx = idx;
   selectedTopic = null;
   document.querySelectorAll('.msg-item').forEach(el => {
      el.classList.toggle('selected', parseInt(el.dataset.idx) === idx);
   });
   renderTopics();
   renderTopologyGraph();
   renderDetail(messages[idx]);
}

function renderTopicDetail(topicName) {
   const el = document.getElementById('detailContent');
   if (!topicName) {
      el.textContent = 'Select a topic to inspect its QoS and endpoint state.';
      return;
   }

   const info = topics[topicName];
   if (!info) {
      el.textContent = 'Topic not found.';
      return;
   }

   const publisherQos = formatQosProfileUsage(info.publisherQosProfiles, 'Publisher');
   const subscriberQos = formatQosProfileUsage(info.subscriberQosProfiles, 'Subscriber');
   el.innerHTML = `<span class="field-key">Topic:</span> ${esc(topicName)}\n\n` +
                  `<span class="field-key">Publisher QoS Profiles:</span>\n${esc(publisherQos)}\n\n` +
                  `<span class="field-key">Subscriber QoS Profiles:</span>\n${esc(subscriberQos)}`;
}

function formatQosProfileUsage(profiles, role) {
   if (!Array.isArray(profiles) || profiles.length === 0)
      return `No ${role.toLowerCase()} endpoints discovered.`;

   const lines = [];
   profiles.forEach((usage, idx) => {
      const profileText = usage && usage.profile ? String(usage.profile) : 'unknown profile';
      const apps = usage && Array.isArray(usage.applications)
         ? usage.applications.map((a) => String(a).trim()).filter(Boolean)
         : [];
      const appText = apps.length ? apps.join(', ') : 'unknown app';
      lines.push(`${idx + 1}. ${profileText}`);
      lines.push(`   apps: ${appText}`);
   });

   return lines.join('\n');
}

function renderDetail(entry) {
   const el = document.getElementById('detailContent');
   if (!entry) { el.textContent = 'Select a message.'; return; }
   el.innerHTML = `<span class="field-key">Topic:</span> ${esc(entry.topic)}\n` +
                  `<span class="field-key">Time:</span>  ${entry.time}\n\n` +
                  formatObject(entry.data, 0);
}

function formatObject(obj, indent) {
   if (obj === null || obj === undefined) return 'null';
   if (typeof obj !== 'object') return esc(String(obj));
   const pad = '  '.repeat(indent);
   let out = '';
   for (const [k, v] of Object.entries(obj)) {
      if (typeof v === 'object' && v !== null) {
         out += `${pad}<span class="field-key">${esc(k)}:</span>\n${formatObject(v, indent + 1)}`;
      } else {
         out += `${pad}<span class="field-key">${esc(k)}:</span> <span class="field-val">${esc(String(v))}</span>\n`;
      }
   }
   return out;
}

function summarize(data) {
   if (!data) return '';
   const parts = [];
   for (const [k, v] of Object.entries(data)) {
      if (typeof v !== 'object') parts.push(`${k}=${v}`);
      if (parts.length >= 3) break;
   }
   return parts.join(', ');
}

function clearMessages() {
   messages = [];
   selectedMsgIdx = -1;
   document.getElementById('messagesList').innerHTML = '';
   document.getElementById('detailContent').textContent = 'Select a message.';
   document.getElementById('msgCount').textContent = '0';
   document.getElementById('footerStats').textContent = '';
   for (const t of Object.values(topics)) t.count = 0;
}

window.addEventListener('resize', () => {
   renderTopologyGraph();
});

// --- Recording ---
function toggleRecord() {
   if (!recording) {
      send({ type: 'record_start' });
   } else {
      send({ type: 'record_stop' });
   }
}

function updateRecUI() {
   const btn = document.getElementById('btnRecord');
   const ind = document.getElementById('recIndicator');
   btn.textContent = recording ? 'Stop' : 'Record';
   btn.classList.toggle('active', recording);
   ind.classList.toggle('active', recording);
}

function openLoadRecording() {
   document.getElementById('loadRecFile').click();
}

function uploadRecording(event) {
   const file = event.target.files[0];
   if (!file) return;
   const reader = new FileReader();
   reader.onload = (e) => {
      const text = e.target.result;
      document.getElementById('footerInfo').textContent =
         `Uploading recording...`;
      fetch('/playback/load', { method: 'POST', body: text })
         .then(resp => {
            if (resp.ok) {
               return resp.json().then(data => {
                  recordingLoaded = true;
                  recordingLineCount = data.lines;
                  updatePlaybackUI();
                  document.getElementById('footerInfo').textContent =
                     `Recording loaded: ${data.lines} messages from ${file.name}`;
               });
            } else {
               resp.text().then(t => {
                  document.getElementById('footerInfo').textContent =
                     `Load error: ${t}`;
               });
            }
         })
         .catch(ex => {
            document.getElementById('footerInfo').textContent =
               `Load error: ${ex.message}`;
         });
   };
   reader.readAsText(file);
   event.target.value = '';
}

function togglePlayback() {
   if (!playing) {
      send({ type: 'playback_start' });
   } else {
      send({ type: 'playback_stop' });
   }
}

function updatePlaybackUI() {
   const btn = document.getElementById('btnPlayback');
   btn.disabled = !recordingLoaded;
   if (playing) {
      btn.textContent = 'Stop';
      btn.classList.add('playing');
   } else {
      btn.textContent = 'Playback';
      btn.classList.remove('playing');
   }
}

function updateProgress(current, total) {
   const bar = document.getElementById('progressBar');
   const fill = document.getElementById('progressFill');
   bar.classList.add('active');
   const pct = (current / total * 100).toFixed(1);
   fill.style.width = pct + '%';
   document.getElementById('footerInfo').textContent =
      `Playback: ${current} / ${total} (${pct}%)`;
}

function resetProgress() {
   const bar = document.getElementById('progressBar');
   const fill = document.getElementById('progressFill');
   bar.classList.remove('active');
   fill.style.width = '0%';
}

function esc(s) {
   const d = document.createElement('div');
   d.textContent = s;
   return d.innerHTML;
}

// Start
connect();
