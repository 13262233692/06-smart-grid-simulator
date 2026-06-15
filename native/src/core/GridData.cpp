#include "core/GridData.h"
#include <stdexcept>

namespace GridSolver {

int GridData::nPQ() const {
    int count = 0;
    for (const auto& bus : buses) {
        if (bus.type == BusType::PQ) count++;
    }
    return count;
}

int GridData::nPV() const {
    int count = 0;
    for (const auto& bus : buses) {
        if (bus.type == BusType::PV) count++;
    }
    return count;
}

int GridData::nSlack() const {
    int count = 0;
    for (const auto& bus : buses) {
        if (bus.type == BusType::SLACK) count++;
    }
    return count;
}

int GridData::findBusIndex(int busId) const {
    auto it = busIdToIndex.find(busId);
    if (it == busIdToIndex.end()) {
        throw std::runtime_error("Bus ID not found: " + std::to_string(busId));
    }
    return it->second;
}

void GridData::buildBusIndexMap() {
    busIdToIndex.clear();
    for (size_t i = 0; i < buses.size(); i++) {
        busIdToIndex[buses[i].id] = static_cast<int>(i);
    }
}

void GridData::validate() const {
    if (buses.empty()) {
        throw std::runtime_error("Grid validation failed: no buses defined");
    }
    if (baseMVA <= 0) {
        throw std::runtime_error("Grid validation failed: baseMVA must be positive");
    }
    int slackCount = 0;
    for (const auto& bus : buses) {
        if (bus.type == BusType::SLACK) slackCount++;
    }
    if (slackCount == 0) {
        throw std::runtime_error("Grid validation failed: no slack bus defined");
    }
    for (const auto& branch : branches) {
        if (busIdToIndex.find(branch.fromBus) == busIdToIndex.end()) {
            throw std::runtime_error("Branch references invalid from bus: " + std::to_string(branch.fromBus));
        }
        if (busIdToIndex.find(branch.toBus) == busIdToIndex.end()) {
            throw std::runtime_error("Branch references invalid to bus: " + std::to_string(branch.toBus));
        }
    }
    for (const auto& gen : generators) {
        if (busIdToIndex.find(gen.busId) == busIdToIndex.end()) {
            throw std::runtime_error("Generator references invalid bus: " + std::to_string(gen.busId));
        }
    }
}

} // namespace GridSolver
