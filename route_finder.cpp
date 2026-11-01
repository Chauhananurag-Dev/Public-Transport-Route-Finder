#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct Edge {
    std::string from;
    std::string to;
    int time;
    std::string type;
    std::string route;
};

struct RouteResult {
    bool ok = false;
    std::string algorithm;
    std::string note;
    std::string error;
    int total = 0;
    int stops = 0;
    std::vector<Edge> path;
};

static std::string trim(const std::string &input) {
    size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) {
        ++start;
    }
    size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }
    return input.substr(start, end - start);
}

static std::string lowerCopy(const std::string &input) {
    std::string output = input;
    std::transform(output.begin(), output.end(), output.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return output;
}

static std::string jsonEscape(const std::string &text) {
    std::ostringstream out;
    for (char c : text) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    out << "\\u" << std::hex << std::uppercase << static_cast<int>(c);
                } else {
                    out << c;
                }
        }
    }
    return out.str();
}

static std::vector<std::string> splitTabs(const std::string &line) {
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, '\t')) {
        parts.push_back(item);
    }
    return parts;
}

class TransportNetwork {
  public:
    explicit TransportNetwork(bool useDefaults = true) {
        if (useDefaults) {
            loadDefaultNetwork();
        }
    }

    bool stationExists(const std::string &name) const {
        return stations_.count(name) > 0;
    }

    bool addStation(const std::string &rawName, std::string &error) {
        std::string name = trim(rawName);
        if (name.empty()) {
            error = "Station name cannot be empty.";
            return false;
        }
        if (stationExists(name)) {
            error = "Station already exists.";
            return false;
        }
        stations_.insert(name);
        adjacency_[name];
        return true;
    }

    bool addConnection(const std::string &fromRaw, const std::string &toRaw, int time,
                       const std::string &typeRaw, const std::string &routeRaw,
                       std::string &error) {
        std::string from = trim(fromRaw);
        std::string to = trim(toRaw);
        std::string type = trim(typeRaw);
        std::string route = trim(routeRaw);

        if (!stationExists(from)) {
            error = "Source station not found.";
            return false;
        }
        if (!stationExists(to)) {
            error = "Destination station not found.";
            return false;
        }
        if (from == to) {
            error = "Source and destination must be different.";
            return false;
        }
        if (time <= 0) {
            error = "Travel time must be greater than 0.";
            return false;
        }
        if (type.empty()) {
            error = "Transport type is required.";
            return false;
        }
        if (route.empty()) {
            error = "Route number is required.";
            return false;
        }

        const std::vector<Edge> &edges = adjacency_[from];
        for (const Edge &edge : edges) {
            if (edge.to == to && edge.type == type && edge.route == route) {
                error = "Duplicate connection already exists.";
                return false;
            }
        }

        adjacency_[from].push_back({from, to, time, type, route});
        return true;
    }

    std::vector<std::string> getStations() const {
        std::vector<std::string> names(stations_.begin(), stations_.end());
        return names;
    }

    std::vector<Edge> getAllConnections() const {
        std::vector<Edge> edges;
        for (const auto &pair : adjacency_) {
            for (const Edge &edge : pair.second) {
                edges.push_back(edge);
            }
        }
        return edges;
    }

    std::vector<std::string> getRoutes() const {
        std::set<std::string> routeIds;
        for (const auto &edge : getAllConnections()) {
            routeIds.insert(edge.route);
        }
        return std::vector<std::string>(routeIds.begin(), routeIds.end());
    }

    std::vector<std::string> getTransportTypes() const {
        std::set<std::string> types;
        for (const auto &edge : getAllConnections()) {
            types.insert(edge.type);
        }
        return std::vector<std::string>(types.begin(), types.end());
    }

    std::vector<std::pair<std::string, std::pair<int, int>>> searchStations(const std::string &queryRaw) const {
        const std::string query = lowerCopy(trim(queryRaw));
        std::map<std::string, int> incoming;
        std::map<std::string, int> outgoing;
        for (const auto &station : stations_) {
            incoming[station] = 0;
            outgoing[station] = 0;
        }

        for (const auto &entry : adjacency_) {
            outgoing[entry.first] += static_cast<int>(entry.second.size());
            for (const auto &edge : entry.second) {
                incoming[edge.to] += 1;
            }
        }

        std::vector<std::pair<std::string, std::pair<int, int>>> matches;
        for (const auto &station : stations_) {
            if (query.empty() || lowerCopy(station).find(query) != std::string::npos) {
                matches.push_back({station, {outgoing[station], incoming[station]}});
            }
        }
        return matches;
    }

    // Dijkstra's algorithm finds the minimum total travel-time path.
    // Each edge weight represents travel time in minutes.
    // A min-priority queue selects the station with the smallest
    // currently known distance.
    RouteResult fastestRoute(const std::string &fromRaw, const std::string &toRaw) const {
        RouteResult result;
        result.algorithm = "dijkstra";
        result.note = "Fastest route — minimum total travel time.";

        const std::string source = trim(fromRaw);
        const std::string destination = trim(toRaw);

        if (source.empty() || destination.empty()) {
            result.error = "Source and destination are required.";
            return result;
        }
        if (!stationExists(source) || !stationExists(destination)) {
            result.error = "Station not found.";
            return result;
        }
        if (source == destination) {
            result.error = "Source and destination must be different.";
            return result;
        }

        const int INF = std::numeric_limits<int>::max() / 4;
        std::map<std::string, int> distance;
        std::map<std::string, Edge> predecessor;
        for (const auto &station : stations_) {
            distance[station] = INF;
        }
        distance[source] = 0;

        std::priority_queue<
            std::pair<int, std::string>,
            std::vector<std::pair<int, std::string>>,
            std::greater<std::pair<int, std::string>>
        > pq;
        pq.push({0, source});

        while (!pq.empty()) {
            auto [currentDistance, currentStation] = pq.top();
            pq.pop();

            if (currentDistance > distance[currentStation]) {
                continue;
            }
            if (currentStation == destination) {
                break;
            }

            auto it = adjacency_.find(currentStation);
            if (it == adjacency_.end()) {
                continue;
            }

            for (const Edge &edge : it->second) {
                int newDistance = currentDistance + edge.time;
                if (newDistance < distance[edge.to]) {
                    distance[edge.to] = newDistance;
                    predecessor[edge.to] = edge;
                    pq.push({newDistance, edge.to});
                }
            }
        }

        if (distance[destination] >= INF) {
            result.error = "No route exists between the selected stations.";
            return result;
        }

        std::vector<Edge> reversed;
        std::string current = destination;
        while (current != source) {
            auto it = predecessor.find(current);
            if (it == predecessor.end()) {
                result.error = "No route exists between the selected stations.";
                return result;
            }
            reversed.push_back(it->second);
            current = it->second.from;
        }

        std::reverse(reversed.begin(), reversed.end());
        result.path = reversed;
        result.total = distance[destination];
        result.stops = static_cast<int>(reversed.size());
        result.ok = true;
        return result;
    }

    // BFS explores the graph level by level.
    // Since each edge represents one connection, BFS finds
    // the path containing the minimum number of connections.
    RouteResult minimumStopsRoute(const std::string &fromRaw, const std::string &toRaw) const {
        RouteResult result;
        result.algorithm = "bfs";
        result.note = "Minimum stops — minimum number of connections.";

        const std::string source = trim(fromRaw);
        const std::string destination = trim(toRaw);

        if (source.empty() || destination.empty()) {
            result.error = "Source and destination are required.";
            return result;
        }
        if (!stationExists(source) || !stationExists(destination)) {
            result.error = "Station not found.";
            return result;
        }
        if (source == destination) {
            result.error = "Source and destination must be different.";
            return result;
        }

        std::queue<std::string> q;
        std::set<std::string> visited;
        std::map<std::string, Edge> predecessor;

        q.push(source);
        visited.insert(source);

        bool found = false;
        while (!q.empty() && !found) {
            std::string current = q.front();
            q.pop();

            auto it = adjacency_.find(current);
            if (it == adjacency_.end()) {
                continue;
            }

            for (const Edge &edge : it->second) {
                if (!visited.count(edge.to)) {
                    visited.insert(edge.to);
                    predecessor[edge.to] = edge;
                    if (edge.to == destination) {
                        found = true;
                        break;
                    }
                    q.push(edge.to);
                }
            }
        }

        if (!found) {
            result.error = "No route exists between the selected stations.";
            return result;
        }

        std::vector<Edge> reversed;
        std::string current = destination;
        int totalTime = 0;

        while (current != source) {
            auto it = predecessor.find(current);
            if (it == predecessor.end()) {
                result.error = "No route exists between the selected stations.";
                return result;
            }
            reversed.push_back(it->second);
            totalTime += it->second.time;
            current = it->second.from;
        }

        std::reverse(reversed.begin(), reversed.end());
        result.path = reversed;
        result.total = totalTime;
        result.stops = static_cast<int>(reversed.size());
        result.ok = true;
        return result;
    }

    std::string statsJson() const {
        std::ostringstream out;
        std::vector<Edge> edges = getAllConnections();
        std::vector<std::string> routes = getRoutes();
        std::vector<std::string> types = getTransportTypes();

        out << "{\"ok\":true";
        out << ",\"stations\":" << stations_.size();
        out << ",\"connections\":" << edges.size();
        out << ",\"routes\":" << routes.size();
        out << ",\"transportTypes\":[";
        for (size_t i = 0; i < types.size(); ++i) {
            if (i) out << ",";
            out << "\"" << jsonEscape(types[i]) << "\"";
        }
        out << "]}";
        return out.str();
    }

    std::string stationsJson() const {
        std::vector<std::string> stations = getStations();
        std::ostringstream out;
        out << "{\"ok\":true,\"stations\":[";
        for (size_t i = 0; i < stations.size(); ++i) {
            if (i) out << ",";
            out << "\"" << jsonEscape(stations[i]) << "\"";
        }
        out << "]}";
        return out.str();
    }

    std::string networkJson() const {
        std::vector<Edge> edges = getAllConnections();
        std::ostringstream out;
        out << "{\"ok\":true,\"connections\":[";
        for (size_t i = 0; i < edges.size(); ++i) {
            if (i) out << ",";
            out << edgeJson(edges[i]);
        }
        out << "]}";
        return out.str();
    }

    std::string routesJson() const {
        std::vector<std::string> routeIds = getRoutes();
        std::ostringstream out;
        out << "{\"ok\":true,\"routes\":[";
        for (size_t i = 0; i < routeIds.size(); ++i) {
            if (i) out << ",";
            out << "\"" << jsonEscape(routeIds[i]) << "\"";
        }
        out << "]}";
        return out.str();
    }

    std::string searchJson(const std::string &query) const {
        auto matches = searchStations(query);
        std::ostringstream out;
        out << "{\"ok\":true,\"query\":\"" << jsonEscape(query) << "\",\"results\":[";
        for (size_t i = 0; i < matches.size(); ++i) {
            if (i) out << ",";
            out << "{\"name\":\"" << jsonEscape(matches[i].first) << "\"";
            out << ",\"outgoing\":" << matches[i].second.first;
            out << ",\"incoming\":" << matches[i].second.second << "}";
        }
        out << "]}";
        return out.str();
    }

    static std::string routeResultJson(const RouteResult &result) {
        std::ostringstream out;
        out << "{\"ok\":" << (result.ok ? "true" : "false");
        out << ",\"algorithm\":\"" << jsonEscape(result.algorithm) << "\"";
        out << ",\"note\":\"" << jsonEscape(result.note) << "\"";
        if (result.ok) {
            out << ",\"total\":" << result.total;
            out << ",\"stops\":" << result.stops;
            out << ",\"path\":[";
            for (size_t i = 0; i < result.path.size(); ++i) {
                if (i) out << ",";
                out << edgeJson(result.path[i]);
            }
            out << "]";
        } else {
            out << ",\"error\":\"" << jsonEscape(result.error) << "\"";
        }
        out << "}";
        return out.str();
    }

    static std::string responseOkMessage(const std::string &message) {
        return std::string("{\"ok\":true,\"message\":\"") + jsonEscape(message) + "\"}";
    }

    static std::string responseError(const std::string &message) {
        return std::string("{\"ok\":false,\"error\":\"") + jsonEscape(message) + "\"}";
    }

  private:
    std::set<std::string> stations_;
    std::map<std::string, std::vector<Edge>> adjacency_;

    static std::string edgeJson(const Edge &edge) {
        std::ostringstream out;
        out << "{\"from\":\"" << jsonEscape(edge.from) << "\"";
        out << ",\"to\":\"" << jsonEscape(edge.to) << "\"";
        out << ",\"time\":" << edge.time;
        out << ",\"type\":\"" << jsonEscape(edge.type) << "\"";
        out << ",\"route\":\"" << jsonEscape(edge.route) << "\"}";
        return out.str();
    }

    void loadDefaultNetwork() {
        std::vector<std::string> stationNames = {
            "Airport Terminal", "Central Station", "City Hall", "Downtown Hub", "East Avenue",
            "Green Valley", "Harbor Point", "Hospital Park", "Industrial Zone", "Lakeside",
            "Market Square", "Museum District", "North Terminal", "Old Town", "Park Drive",
            "Riverside Station", "Stadium", "Sunset Plaza", "Tech Center", "University Square", "West End"
        };

        for (const auto &station : stationNames) {
            std::string ignored;
            addStation(station, ignored);
        }

        auto addDefaultConnection = [&](const std::string &from, const std::string &to, int time,
                                        const std::string &type, const std::string &route) {
            std::string ignored;
            addConnection(from, to, time, type, route, ignored);
        };

        addDefaultConnection("Airport Terminal", "Central Station", 12, "Metro", "M1");
        addDefaultConnection("Central Station", "City Hall", 5, "Bus", "B1");
        addDefaultConnection("City Hall", "Market Square", 4, "Bus", "B2");
        addDefaultConnection("Market Square", "University Square", 6, "Bus", "B3");
        addDefaultConnection("Central Station", "Downtown Hub", 3, "Metro", "M2");
        addDefaultConnection("Downtown Hub", "Tech Center", 7, "Metro", "M3");
        addDefaultConnection("Tech Center", "University Square", 4, "Tram", "T1");
        addDefaultConnection("Central Station", "Old Town", 6, "Bus", "B4");
        addDefaultConnection("Old Town", "Museum District", 5, "Tram", "T2");
        addDefaultConnection("Museum District", "City Hall", 4, "Tram", "T3");
        addDefaultConnection("East Avenue", "City Hall", 7, "Bus", "B5");
        addDefaultConnection("West End", "Central Station", 8, "Bus", "B6");
        addDefaultConnection("North Terminal", "Central Station", 9, "Metro", "M4");
        addDefaultConnection("Harbor Point", "Downtown Hub", 6, "Ferry", "F1");
        addDefaultConnection("Lakeside", "Harbor Point", 5, "Tram", "T4");
        addDefaultConnection("Green Valley", "Lakeside", 6, "Bus", "B7");
        addDefaultConnection("Hospital Park", "City Hall", 5, "Bus", "B8");
        addDefaultConnection("Industrial Zone", "Tech Center", 8, "Bus", "B9");
        addDefaultConnection("Park Drive", "Stadium", 4, "Tram", "T5");
        addDefaultConnection("Stadium", "Downtown Hub", 5, "Metro", "M5");
        addDefaultConnection("Sunset Plaza", "Park Drive", 3, "Bus", "B10");
        addDefaultConnection("Riverside Station", "Market Square", 7, "Tram", "T6");
        addDefaultConnection("University Square", "Hospital Park", 5, "Bus", "B11");
        addDefaultConnection("Downtown Hub", "Riverside Station", 6, "Bus", "B12");
        addDefaultConnection("City Hall", "Airport Terminal", 15, "Express", "X1");
        addDefaultConnection("Tech Center", "Industrial Zone", 7, "Metro", "M6");
    }
};

static std::string processCommand(TransportNetwork &network, const std::string &line) {
    const std::vector<std::string> parts = splitTabs(line);
    if (parts.empty()) {
        return TransportNetwork::responseError("Empty command.");
    }

    const std::string command = parts[0];
    if (command == "STATS") {
        return network.statsJson();
    }
    if (command == "STATIONS") {
        return network.stationsJson();
    }
    if (command == "NETWORK") {
        return network.networkJson();
    }
    if (command == "ROUTES") {
        return network.routesJson();
    }
    if (command == "SEARCH") {
        if (parts.size() < 2) {
            return TransportNetwork::responseError("Search query is required.");
        }
        return network.searchJson(parts[1]);
    }
    if (command == "FAST") {
        if (parts.size() < 3) {
            return TransportNetwork::responseError("FAST requires source and destination.");
        }
        return TransportNetwork::routeResultJson(network.fastestRoute(parts[1], parts[2]));
    }
    if (command == "MIN") {
        if (parts.size() < 3) {
            return TransportNetwork::responseError("MIN requires source and destination.");
        }
        return TransportNetwork::routeResultJson(network.minimumStopsRoute(parts[1], parts[2]));
    }
    if (command == "ADDSTATION") {
        if (parts.size() < 2) {
            return TransportNetwork::responseError("ADDSTATION requires a station name.");
        }
        std::string error;
        if (!network.addStation(parts[1], error)) {
            return TransportNetwork::responseError(error);
        }
        return TransportNetwork::responseOkMessage("Station added successfully.");
    }
    if (command == "ADDCONN") {
        if (parts.size() < 6) {
            return TransportNetwork::responseError("ADDCONN requires from, to, time, type, and route.");
        }

        int time = 0;
        try {
            time = std::stoi(parts[3]);
        } catch (...) {
            return TransportNetwork::responseError("Travel time must be a valid integer.");
        }

        std::string error;
        if (!network.addConnection(parts[1], parts[2], time, parts[4], parts[5], error)) {
            return TransportNetwork::responseError(error);
        }
        return TransportNetwork::responseOkMessage("Connection added successfully.");
    }

    return TransportNetwork::responseError("Unknown command.");
}

#ifndef TRANSITFLOW_TEST
static int runConsoleMode() {
    TransportNetwork network;
    std::cout << "TransitFlow Routing Engine (Console Mode)\n";
    std::cout << "Type commands: STATS, STATIONS, NETWORK, FAST<TAB>from<TAB>to, MIN<TAB>from<TAB>to\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "EXIT" || line == "QUIT") {
            break;
        }
        std::cout << processCommand(network, line) << std::endl;
    }

    return 0;
}

static int runApiMode() {
    TransportNetwork network;
    std::string line;
    while (std::getline(std::cin, line)) {
        const std::string response = processCommand(network, line);
        std::cout << response << std::endl;
        std::cout.flush();
    }
    return 0;
}

int main(int argc, char **argv) {
    bool apiMode = (argc > 1 && std::string(argv[1]) == "--api");
    return apiMode ? runApiMode() : runConsoleMode();
}
#endif
