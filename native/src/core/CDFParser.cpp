#include "core/CDFParser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace GridSolver {

CDFParser::CDFParser() {}
CDFParser::~CDFParser() {}

std::string CDFParser::trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
        start++;
    }
    auto end = s.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(static_cast<unsigned char>(*end)));
    return std::string(start, end + 1);
}

GridData CDFParser::parse(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw ParseError("Cannot open file: " + filePath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    GridData grid = parseString(buffer.str());
    grid.sourceFile = filePath;
    return grid;
}

GridData CDFParser::parseString(const std::string& content) {
    GridData grid;
    std::istringstream stream(content);
    std::string line;

    std::getline(stream, line);
    parseTitleData(grid, line);

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        if (trimmed.find("BUS DATA FOLLOWS") != std::string::npos) {
            parseBusData(grid, stream);
        } else if (trimmed.find("BRANCH DATA FOLLOWS") != std::string::npos) {
            parseBranchData(grid, stream);
        } else if (trimmed.find("-999") != std::string::npos) {
            continue;
        }
    }

    grid.buildBusIndexMap();
    grid.validate();
    return grid;
}

void CDFParser::parseTitleData(GridData& grid, const std::string& line) {
    if (line.size() < 32) {
        grid.caseName = trim(line.substr(0, line.size()));
        grid.baseMVA = 100.0;
        return;
    }

    grid.caseName = trim(line.substr(0, 30));

    std::string mvaStr = trim(line.substr(30, 12));
    if (!mvaStr.empty()) {
        try {
            grid.baseMVA = std::stod(mvaStr);
        } catch (...) {
            grid.baseMVA = 100.0;
        }
    }
    if (grid.baseMVA <= 0) grid.baseMVA = 100.0;
}

void CDFParser::parseBusData(GridData& grid, std::ifstream& file) {
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("-999") != std::string::npos) break;
        if (trim(line).empty()) continue;
        if (line.size() < 100) continue;

        Bus bus;
        try {
            bus.id = std::stoi(trim(line.substr(0, 4)));
            bus.name = trim(line.substr(5, 8));
            bus.area = std::stoi(trim(line.substr(13, 3)));
            bus.baseKV = std::stod(trim(line.substr(18, 6)));
            bus.zone = std::stoi(trim(line.substr(24, 3)));

            int typeCode = std::stoi(trim(line.substr(27, 2)));
            switch (typeCode) {
                case 0: bus.type = BusType::SLACK; break;
                case 2: bus.type = BusType::PV; break;
                default: bus.type = BusType::PQ;
            }

            bus.Vm = std::stod(trim(line.substr(29, 7)));
            if (bus.Vm <= 0.01) bus.Vm = 1.0;

            bus.Va = std::stod(trim(line.substr(36, 8)));
            bus.Va = bus.Va * M_PI / 180.0;

            bus.Pd = std::stod(trim(line.substr(44, 9)));
            bus.Qd = std::stod(trim(line.substr(53, 9)));
            bus.shuntG = std::stod(trim(line.substr(62, 9)));
            bus.shuntB = std::stod(trim(line.substr(71, 9)));
            bus.VmMax = std::stod(trim(line.substr(80, 7)));
            bus.VmMin = std::stod(trim(line.substr(87, 7)));

            bus.Pg = std::stod(trim(line.substr(94, 9)));
            bus.Qg = std::stod(trim(line.substr(103, 9)));
            bus.QgMax = std::stod(trim(line.substr(112, 9)));
            bus.QgMin = std::stod(trim(line.substr(121, 9)));

            bus.x = 0;
            bus.y = 0;

            if (bus.VmMax <= 0) bus.VmMax = 1.1;
            if (bus.VmMin <= 0) bus.VmMin = 0.9;
            if (bus.QgMax == 0 && bus.type == BusType::PV) bus.QgMax = 9999;

        } catch (std::exception& e) {
            continue;
        }

        grid.buses.push_back(bus);

        if (bus.type == BusType::PV || bus.type == BusType::SLACK) {
            if (bus.Pg > 0 || bus.type == BusType::SLACK) {
                Generator gen;
                gen.id = grid.generators.size() + 1;
                gen.busId = bus.id;
                gen.Pg = bus.Pg;
                gen.Qg = bus.Qg;
                gen.PgMax = bus.QgMax > 0 ? 500 : 100;
                gen.PgMin = 0;
                gen.QgMax = bus.QgMax;
                gen.QgMin = bus.QgMin;
                gen.Vset = bus.Vm;
                gen.status = 1;
                grid.generators.push_back(gen);
            }
        }
    }
}

void CDFParser::parseBranchData(GridData& grid, std::ifstream& file) {
    std::string line;
    int branchId = 0;
    while (std::getline(file, line)) {
        if (line.find("-999") != std::string::npos) break;
        if (trim(line).empty()) continue;
        if (line.size() < 60) continue;

        Branch branch;
        try {
            branch.id = ++branchId;
            branch.fromBus = std::stoi(trim(line.substr(0, 4)));
            branch.toBus = std::stoi(trim(line.substr(5, 4)));
            branch.area = std::stoi(trim(line.substr(9, 3)));
            branch.zone = std::stoi(trim(line.substr(12, 3)));

            branch.r = std::stod(trim(line.substr(15, 9)));
            branch.x = std::stod(trim(line.substr(24, 9)));
            branch.b = std::stod(trim(line.substr(33, 9)));

            branch.rateA = std::stod(trim(line.substr(42, 7)));
            branch.rateB = std::stod(trim(line.substr(49, 7)));
            branch.rateC = std::stod(trim(line.substr(56, 7)));

            branch.tap = line.size() >= 66 ? std::stod(trim(line.substr(63, 6))) : 0.0;
            branch.phaseShift = line.size() >= 73 ? std::stod(trim(line.substr(69, 6))) : 0.0;
            branch.phaseShift = branch.phaseShift * M_PI / 180.0;

            branch.status = 1;

            branch.ratioMin = 0.9;
            branch.ratioMax = 1.1;

            if (branch.tap == 0) branch.tap = 1.0;

        } catch (std::exception& e) {
            continue;
        }

        grid.branches.push_back(branch);
    }
}

} // namespace GridSolver
