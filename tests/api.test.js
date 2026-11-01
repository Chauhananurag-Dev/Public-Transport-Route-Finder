const test = require('node:test');
const assert = require('node:assert/strict');

const { app, bridge } = require('../server');

let server;
let baseUrl;

test.before(async () => {
  server = app.listen(0);
  const { port } = server.address();
  baseUrl = `http://127.0.0.1:${port}`;
});

test.after(async () => {
  server.close();
  bridge.stop();
});

async function request(path, options) {
  const response = await fetch(`${baseUrl}${path}`, options);
  const json = await response.json();
  return { status: response.status, body: json };
}

test('health endpoint works', async () => {
  const { status, body } = await request('/api/health');
  assert.equal(status, 200);
  assert.equal(body.ok, true);
});

test('stations and stats endpoints return expected shape', async () => {
  const stations = await request('/api/stations');
  const stats = await request('/api/stats');

  assert.equal(stations.status, 200);
  assert.ok(Array.isArray(stations.body.stations));
  assert.ok(stations.body.stations.length >= 21);

  assert.equal(stats.status, 200);
  assert.equal(stats.body.ok, true);
  assert.ok(stats.body.connections >= 26);
});

test('route validation for same source and destination', async () => {
  const { status, body } = await request('/api/route/fast?from=Central%20Station&to=Central%20Station');
  assert.equal(status, 400);
  assert.equal(body.ok, false);
});

test('dijkstra and bfs can return different routes', async () => {
  const stations = ['A', 'B', 'C', 'D', 'E'];
  for (const name of stations) {
    await request('/api/stations', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name })
    });
  }

  const connections = [
    ['A', 'B', 10, 'Bus', 'R1'],
    ['B', 'D', 10, 'Bus', 'R2'],
    ['A', 'C', 3, 'Metro', 'R3'],
    ['C', 'E', 3, 'Metro', 'R4'],
    ['E', 'D', 4, 'Metro', 'R5']
  ];

  for (const [from, to, time, type, route] of connections) {
    await request('/api/connections', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ from, to, time, type, route })
    });
  }

  const fast = await request('/api/route/fast?from=A&to=D');
  const min = await request('/api/route/min?from=A&to=D');

  assert.equal(fast.status, 200);
  assert.equal(min.status, 200);

  assert.equal(fast.body.ok, true);
  assert.equal(min.body.ok, true);

  assert.equal(fast.body.total, 10);
  assert.equal(fast.body.stops, 3);
  assert.equal(min.body.total, 20);
  assert.equal(min.body.stops, 2);
});
