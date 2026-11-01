const fromStation = document.getElementById('fromStation');
const toStation = document.getElementById('toStation');
const connFrom = document.getElementById('connFrom');
const connTo = document.getElementById('connTo');
const routeResult = document.getElementById('routeResult');
const findRouteBtn = document.getElementById('findRouteBtn');
const adminMessage = document.getElementById('adminMessage');

async function api(url, options = {}) {
  const res = await fetch(url, options);
  const payload = await res.json();
  if (!res.ok) {
    throw new Error(payload.error || 'Request failed');
  }
  return payload;
}

function optionList(select, stations) {
  select.innerHTML = '<option value="">Select station</option>';
  stations.forEach((name) => {
    const option = document.createElement('option');
    option.value = name;
    option.textContent = name;
    select.appendChild(option);
  });
}

async function loadHealth() {
  const label = document.getElementById('engineStatus');
  try {
    const health = await api('/api/health');
    label.textContent = `Routing Engine: ● ${health.routingEngine === 'connected' ? 'Online' : 'Offline'}`;
  } catch {
    label.textContent = 'Routing Engine: ● Offline';
  }
}

async function loadStats() {
  const stats = await api('/api/stats');
  const el = document.getElementById('stats');
  el.innerHTML = '';
  const cards = [
    ['Stations', stats.stations],
    ['Connections', stats.connections],
    ['Routes', stats.routes],
    ['Transport', (stats.transportTypes || []).join(' / ') || 'N/A']
  ];
  cards.forEach(([label, value]) => {
    const card = document.createElement('div');
    card.className = 'stat';
    card.innerHTML = `<strong>${value}</strong><div>${label}</div>`;
    el.appendChild(card);
  });
}

async function loadStations() {
  const data = await api('/api/stations');
  optionList(fromStation, data.stations);
  optionList(toStation, data.stations);
  optionList(connFrom, data.stations);
  optionList(connTo, data.stations);
}

async function loadNetwork() {
  const data = await api('/api/network');
  const container = document.getElementById('networkResults');
  if (!data.connections || data.connections.length === 0) {
    container.textContent = 'No network connections available.';
    return;
  }

  container.innerHTML = data.connections
    .map((c) => `<div class="route-leg">${c.from} → ${c.to}<br>${c.type} ${c.route} • ${c.time} min</div>`)
    .join('');
}

function renderRoute(data) {
  if (!data.ok) {
    routeResult.className = 'result error';
    routeResult.textContent = data.error || 'Unable to calculate route.';
    return;
  }

  const legs = data.path
    .map((leg, index) => `<div class="route-leg">${index + 1}. ${leg.from} → ${leg.to}<br>${leg.type} ${leg.route} • ${leg.time} min</div>`)
    .join('');

  routeResult.className = 'result';
  routeResult.innerHTML = `
    <h3>${data.algorithm === 'dijkstra' ? 'Fastest Route' : 'Minimum Stops Route'}</h3>
    <p>${data.note}</p>
    ${legs}
    <p><strong>Total time:</strong> ${data.total} minutes</p>
    <p><strong>Stops:</strong> ${data.stops}</p>
  `;
}

async function findRoute(event) {
  event.preventDefault();
  const from = fromStation.value;
  const to = toStation.value;
  const mode = document.querySelector('input[name="mode"]:checked').value;

  if (!from || !to) {
    renderRoute({ ok: false, error: 'Please select both source and destination stations.' });
    return;
  }
  if (from === to) {
    renderRoute({ ok: false, error: 'Source and destination must be different.' });
    return;
  }

  findRouteBtn.disabled = true;
  routeResult.className = 'result';
  routeResult.textContent = 'Calculating route...';

  try {
    const endpoint = mode === 'fast' ? '/api/route/fast' : '/api/route/min';
    const data = await api(`${endpoint}?from=${encodeURIComponent(from)}&to=${encodeURIComponent(to)}`);
    renderRoute(data);
  } catch (error) {
    renderRoute({ ok: false, error: error.message || 'Unable to calculate the route. Please try again.' });
  } finally {
    findRouteBtn.disabled = false;
  }
}

async function searchStation(event) {
  event.preventDefault();
  const query = document.getElementById('searchQuery').value;
  const holder = document.getElementById('searchResults');

  try {
    const data = await api(`/api/search?q=${encodeURIComponent(query)}`);
    if (!data.results.length) {
      holder.textContent = 'No stations match your search.';
      return;
    }
    holder.innerHTML = data.results.map((s) => `<div class="route-leg"><strong>${s.name}</strong><br>Outgoing: ${s.outgoing}, Incoming: ${s.incoming}</div>`).join('');
  } catch (error) {
    holder.textContent = error.message;
  }
}

async function addStation(event) {
  event.preventDefault();
  adminMessage.textContent = '';
  const name = document.getElementById('stationName').value.trim();
  if (!name) {
    adminMessage.textContent = 'Station name cannot be empty.';
    adminMessage.className = 'error';
    return;
  }

  try {
    const data = await api('/api/stations', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name })
    });
    adminMessage.textContent = data.message || 'Station added successfully.';
    adminMessage.className = 'success';
    event.target.reset();
    await Promise.all([loadStations(), loadStats()]);
  } catch (error) {
    adminMessage.textContent = error.message;
    adminMessage.className = 'error';
  }
}

async function addConnection(event) {
  event.preventDefault();
  adminMessage.textContent = '';

  const payload = {
    from: connFrom.value,
    to: connTo.value,
    time: Number(document.getElementById('connTime').value),
    type: document.getElementById('connType').value.trim(),
    route: document.getElementById('connRoute').value.trim()
  };

  try {
    const data = await api('/api/connections', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    adminMessage.textContent = data.message || 'Connection added successfully.';
    adminMessage.className = 'success';
    event.target.reset();
    await Promise.all([loadNetwork(), loadStats()]);
  } catch (error) {
    adminMessage.textContent = error.message;
    adminMessage.className = 'error';
  }
}

document.getElementById('routeForm').addEventListener('submit', findRoute);
document.getElementById('searchForm').addEventListener('submit', searchStation);
document.getElementById('addStationForm').addEventListener('submit', addStation);
document.getElementById('addConnectionForm').addEventListener('submit', addConnection);

Promise.all([loadHealth(), loadStats(), loadStations(), loadNetwork()]).catch((error) => {
  routeResult.className = 'result error';
  routeResult.textContent = error.message || 'Failed to initialize TransitFlow.';
});
