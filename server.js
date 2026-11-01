const express = require('express');
const path = require('path');
const { spawn } = require('child_process');

const PORT = Number(process.env.PORT) || 3000;
const ENGINE_TIMEOUT_MS = Number(process.env.ENGINE_TIMEOUT_MS) || 5000;

class RoutingEngineBridge {
  constructor() {
    this.proc = null;
    this.buffer = '';
    this.pending = [];
    this.available = false;
  }

  start() {
    const basePath = path.resolve(__dirname, process.platform === 'win32' ? 'route_finder.exe' : 'route_finder');
    const exePath = process.platform === 'win32' ? basePath : basePath;

    console.log('[INFO] Starting routing engine');
    this.proc = spawn(exePath, ['--api'], {
      stdio: ['pipe', 'pipe', 'pipe']
    });

    this.available = true;

    this.proc.stdout.setEncoding('utf8');
    this.proc.stdout.on('data', (chunk) => this.handleStdout(chunk));

    this.proc.stderr.setEncoding('utf8');
    this.proc.stderr.on('data', (chunk) => {
      const message = chunk.toString().trim();
      if (message) {
        console.error(`[ERROR] routing-engine: ${message}`);
      }
    });

    this.proc.on('exit', (code, signal) => {
      console.error(`[ERROR] Routing engine exited (code=${code}, signal=${signal})`);
      this.available = false;
      this.rejectAllPending('Routing engine unavailable');
    });

    this.proc.on('error', (error) => {
      console.error(`[ERROR] Failed to start routing engine: ${error.message}`);
      this.available = false;
      this.rejectAllPending('Routing engine failed to start');
    });

    console.log('[INFO] Routing engine ready');
  }

  stop() {
    if (this.proc && !this.proc.killed) {
      this.proc.kill();
    }
    this.available = false;
  }

  isAvailable() {
    return this.available && this.proc && !this.proc.killed;
  }

  rejectAllPending(message) {
    while (this.pending.length > 0) {
      const item = this.pending.shift();
      item.reject(new Error(message));
    }
  }

  handleStdout(chunk) {
    this.buffer += chunk;
    const lines = this.buffer.split(/\r?\n/);
    this.buffer = lines.pop();

    for (const rawLine of lines) {
      const line = rawLine.trim();
      if (!line) {
        continue;
      }
      const request = this.pending.shift();
      if (!request) {
        continue;
      }
      try {
        const parsed = JSON.parse(line);
        request.resolve(parsed);
      } catch {
        request.reject(new Error('Invalid JSON response from routing engine.'));
      }
    }
  }

  sendCommand(command) {
    if (!this.isAvailable()) {
      return Promise.reject(new Error('Routing engine unavailable'));
    }

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        const idx = this.pending.findIndex((entry) => entry.reject === reject);
        if (idx >= 0) {
          this.pending.splice(idx, 1);
        }
        reject(new Error('Routing engine timeout'));
      }, ENGINE_TIMEOUT_MS);

      this.pending.push({
        resolve: (value) => {
          clearTimeout(timer);
          resolve(value);
        },
        reject: (error) => {
          clearTimeout(timer);
          reject(error);
        }
      });

      this.proc.stdin.write(`${command}\n`);
    });
  }
}

const bridge = new RoutingEngineBridge();
bridge.start();

const app = express();
app.use(express.json());
app.use(express.static(path.resolve(__dirname, 'public')));

function stationInputError(from, to) {
  if (!from || !to) return 'Both source and destination are required.';
  if (from === to) return 'Source and destination must be different.';
  return null;
}

function sendEngineError(res, error) {
  const msg = String(error && error.message ? error.message : error);
  if (msg.includes('timeout')) {
    return res.status(503).json({ ok: false, error: 'Routing engine timeout. Please try again.' });
  }
  return res.status(503).json({ ok: false, error: 'Routing engine unavailable' });
}

app.get('/api/health', (_req, res) => {
  const connected = bridge.isAvailable();
  res.status(connected ? 200 : 503).json({
    ok: connected,
    service: 'TransitFlow',
    routingEngine: connected ? 'connected' : 'unavailable'
  });
});

app.get('/api/stats', async (_req, res) => {
  console.log('[INFO] GET /api/stats');
  try {
    const response = await bridge.sendCommand('STATS');
    res.json(response);
  } catch (error) {
    sendEngineError(res, error);
  }
});

app.get('/api/stations', async (_req, res) => {
  console.log('[INFO] GET /api/stations');
  try {
    const response = await bridge.sendCommand('STATIONS');
    const stations = Array.isArray(response.stations)
      ? [...response.stations].sort((a, b) => a.localeCompare(b))
      : [];
    res.json({ ...response, stations });
  } catch (error) {
    sendEngineError(res, error);
  }
});

app.get('/api/network', async (_req, res) => {
  console.log('[INFO] GET /api/network');
  try {
    res.json(await bridge.sendCommand('NETWORK'));
  } catch (error) {
    sendEngineError(res, error);
  }
});

app.get('/api/routes', async (_req, res) => {
  console.log('[INFO] GET /api/routes');
  try {
    res.json(await bridge.sendCommand('ROUTES'));
  } catch (error) {
    sendEngineError(res, error);
  }
});

app.get('/api/search', async (req, res) => {
  const query = String(req.query.q || '');
  console.log(`[INFO] GET /api/search?q=${query}`);
  try {
    res.json(await bridge.sendCommand(`SEARCH\t${query}`));
  } catch (error) {
    sendEngineError(res, error);
  }
});

async function handleRoute(req, res, mode) {
  const from = String(req.query.from || '').trim();
  const to = String(req.query.to || '').trim();

  const validationError = stationInputError(from, to);
  if (validationError) {
    return res.status(400).json({ ok: false, error: validationError });
  }

  console.log(`[INFO] ${mode} request: ${from} -> ${to}`);

  try {
    const command = `${mode}\t${from}\t${to}`;
    const response = await bridge.sendCommand(command);
    if (!response.ok) {
      const errorMessage = response.error || 'Unable to calculate route';
      const status = errorMessage.includes('not found') ? 404 : 200;
      return res.status(status).json(response);
    }
    return res.json(response);
  } catch (error) {
    return sendEngineError(res, error);
  }
}

app.get('/api/route/fast', async (req, res) => handleRoute(req, res, 'FAST'));
app.get('/api/route/min', async (req, res) => handleRoute(req, res, 'MIN'));

app.post('/api/stations', async (req, res) => {
  const name = String(req.body?.name || '').trim();
  if (!name) {
    return res.status(400).json({ ok: false, error: 'Station name cannot be empty.' });
  }

  try {
    const response = await bridge.sendCommand(`ADDSTATION\t${name}`);
    if (!response.ok) {
      return res.status(response.error && response.error.includes('exists') ? 400 : 400).json(response);
    }
    return res.status(201).json(response);
  } catch (error) {
    return sendEngineError(res, error);
  }
});

app.post('/api/connections', async (req, res) => {
  const from = String(req.body?.from || '').trim();
  const to = String(req.body?.to || '').trim();
  const type = String(req.body?.type || '').trim();
  const route = String(req.body?.route || '').trim();
  const time = Number(req.body?.time);

  if (!from || !to || !type || !route || !Number.isInteger(time)) {
    return res.status(400).json({ ok: false, error: 'from, to, time, type, and route are required.' });
  }
  if (from === to) {
    return res.status(400).json({ ok: false, error: 'Source and destination must be different.' });
  }
  if (time <= 0) {
    return res.status(400).json({ ok: false, error: 'Travel time must be greater than 0.' });
  }

  try {
    const response = await bridge.sendCommand(`ADDCONN\t${from}\t${to}\t${time}\t${type}\t${route}`);
    if (!response.ok) {
      const errorMessage = response.error || '';
      const status = errorMessage.includes('not found') ? 404 : 400;
      return res.status(status).json(response);
    }
    return res.status(201).json(response);
  } catch (error) {
    return sendEngineError(res, error);
  }
});

app.use('/api', (_req, res) => {
  res.status(404).json({ ok: false, error: 'API endpoint not found' });
});

app.get('*', (_req, res) => {
  res.sendFile(path.resolve(__dirname, 'public/index.html'));
});

let server = null;
if (require.main === module) {
  server = app.listen(PORT, () => {
    console.log(`[INFO] TransitFlow running on http://localhost:${PORT}`);
  });
}

function gracefulShutdown() {
  if (server) {
    server.close(() => {
      bridge.stop();
      process.exit(0);
    });
  } else {
    bridge.stop();
    process.exit(0);
  }
}

process.on('SIGTERM', gracefulShutdown);
process.on('SIGINT', gracefulShutdown);

module.exports = { app, bridge };
