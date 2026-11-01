# TransitFlow – Public Transport Route Finder

TransitFlow is a graph-based route planning web app that uses a **C++ routing engine** and a **Node.js API/UI layer**.

## Overview

- Stations are graph vertices.
- Public transport connections are directed weighted edges.
- Edge weight = travel time in minutes.
- The C++ engine is the source of truth for all route calculations.

## Implemented Features

- Fastest route search (Dijkstra)
- Minimum-stop route search (BFS)
- Station search (case-insensitive partial match)
- Network statistics dashboard
- Station list and connection list views
- Add station API + UI
- Add connection API + UI
- Health check endpoint
- Input validation and error handling in backend and frontend

## Algorithms

| Requirement | Algorithm | Data Structure | Optimization |
|-------------|-----------|----------------|-------------|
| Fastest route | Dijkstra | Min-priority queue | Total travel time |
| Minimum stops | BFS | Queue | Number of edges |

- **BFS complexity**: `O(V + E)`
- **Dijkstra complexity (binary heap)**: `O((V + E) log V)`

## Graph Representation

- Vertex/node: station
- Directed edge: transport connection
- Edge fields: `from`, `to`, `time`, `type`, `route`
- Representation: adjacency list (`map<string, vector<Edge>>`)

## Architecture

Browser → Node.js → C++ (`route_finder --api`) → Dijkstra/BFS → JSON → Node.js → Browser

## API Endpoints

### Health
- `GET /api/health`

### Network data
- `GET /api/stats`
- `GET /api/stations`
- `GET /api/network`
- `GET /api/routes`
- `GET /api/search?q=<query>`

### Routing
- `GET /api/route/fast?from=<station>&to=<station>`
  - Dijkstra: minimum total travel time
- `GET /api/route/min?from=<station>&to=<station>`
  - BFS: minimum number of connections

### Administration
- `POST /api/stations`
  - body: `{ "name": "New Station" }`
- `POST /api/connections`
  - body: `{ "from": "A", "to": "B", "time": 5, "type": "Bus", "route": "B1" }`

## Build & Run

### Linux/macOS

```bash
g++ -std=c++17 -O2 route_finder.cpp -o route_finder
./route_finder
./route_finder --api
npm install
npm start
```

### Windows (MinGW)

```bash
g++ -std=c++17 -O2 route_finder.cpp -o route_finder.exe
route_finder.exe
route_finder.exe --api
npm install
npm start
```

Open: `http://localhost:3000`

## Tests

```bash
npm test
```

Covers:
- Dijkstra route cases
- BFS route cases
- Validation cases (same station, invalid station, no route)
- API endpoint checks
- Explicit test proving Dijkstra and BFS can return different paths

## Data Persistence

The network is in-memory.

Changes persist while the routing engine is running and reset on process restart.

## Future Scope

1. GTFS timetable integration
2. Live transit delays
3. Service disruption handling
4. Fare optimization
5. Walking-distance optimization
6. Interactive map integration
7. A* search for larger networks (heuristic-guided)
8. Database-backed persistence
9. Real-time vehicle tracking
10. Multi-modal route optimization
11. User accounts and saved routes
12. Mobile application
