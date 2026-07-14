const WS_URL = `ws://${location.host}/ws`;
let ws = null;
let topics = {};          // {name: {subscribed: bool, count: int}}
let messages = [];
let selectedMsgIdx = -1;
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
         topics[t.name] = { subscribed: t.subscribed, transport: t.transport, count: 0 };
      } else {
         topics[t.name].subscribed = t.subscribed;
         topics[t.name].transport = t.transport;
      }
   }
   renderTopics();
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
         div.className = 'topic-item';
         div.innerHTML = `
            <span class="topic-name">${esc(name)}</span>
            <span class="topic-badge ${info.subscribed ? 'subscribed' : ''}">
               ${info.subscribed ? 'SUB' : 'OFF'}
            </span>`;
         div.oncontextmenu = (e) => { e.preventDefault(); showCtxMenu(e, name); };
         div.onclick = () => toggleSubscribe(name);
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

   const preview = summarize(entry.data);
   div.innerHTML = `
      <span class="msg-topic">${esc(entry.topic)}</span>
      <span class="msg-time">${timeStr}</span>
      <span class="msg-preview">${esc(preview)}</span>`;
   div.onclick = () => selectMessage(idx);
   el.appendChild(div);

   // Auto-scroll to bottom
   el.scrollTop = el.scrollHeight;
}

function selectMessage(idx) {
   selectedMsgIdx = idx;
   document.querySelectorAll('.msg-item').forEach(el => {
      el.classList.toggle('selected', parseInt(el.dataset.idx) === idx);
   });
   renderDetail(messages[idx]);
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
