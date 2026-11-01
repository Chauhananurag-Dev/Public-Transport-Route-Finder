#define TRANSITFLOW_TEST
#include "../route_finder.cpp"

#include <cassert>
#include <iostream>

void testDijkstraAndBfsDifference() {
    TransportNetwork network(false);
    std::string err;

    network.addStation("A", err);
    network.addStation("B", err);
    network.addStation("C", err);
    network.addStation("D", err);
    network.addStation("E", err);

    network.addConnection("A", "B", 10, "Bus", "R1", err);
    network.addConnection("B", "D", 10, "Bus", "R2", err);

    network.addConnection("A", "C", 3, "Metro", "R3", err);
    network.addConnection("C", "E", 3, "Metro", "R4", err);
    network.addConnection("E", "D", 4, "Metro", "R5", err);

    RouteResult fastest = network.fastestRoute("A", "D");
    RouteResult minStops = network.minimumStopsRoute("A", "D");

    assert(fastest.ok);
    assert(minStops.ok);
    assert(fastest.total == 10);
    assert(fastest.stops == 3);
    assert(minStops.total == 20);
    assert(minStops.stops == 2);
}

void testValidationAndNoPath() {
    TransportNetwork network(false);
    std::string err;

    assert(!network.addStation("   ", err));
    assert(!err.empty());

    err.clear();
    assert(network.addStation("X", err));
    assert(network.addStation("Y", err));

    RouteResult sameStation = network.fastestRoute("X", "X");
    assert(!sameStation.ok);

    RouteResult noPath = network.minimumStopsRoute("X", "Y");
    assert(!noPath.ok);

    err.clear();
    assert(!network.addConnection("X", "X", 2, "Bus", "R", err));
}

void testDefaultNetworkRoute() {
    TransportNetwork network;
    RouteResult fastest = network.fastestRoute("Central Station", "University Square");
    RouteResult minStops = network.minimumStopsRoute("Central Station", "University Square");

    assert(fastest.ok);
    assert(minStops.ok);
    assert(fastest.total <= minStops.total);
    assert(minStops.stops <= fastest.stops);
}

int main() {
    testDijkstraAndBfsDifference();
    testValidationAndNoPath();
    testDefaultNetworkRoute();
    std::cout << "All C++ tests passed.\n";
    return 0;
}
