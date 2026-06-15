#pragma once
#include <vector>
#include <string>
#include <complex>
#include <map>

namespace GridSolver {

using Complex = std::complex<double>;

enum class BusType {
    SLACK = 0,
    PV = 1,
    PQ = 2
};

struct Bus {
    int id;
    std::string name;
    BusType type;
    double baseKV;
    int area;
    int zone;

    double Vm;
    double Va;
    double VmMax;
    double VmMin;

    double Pd;
    double Qd;

    double Pg;
    double Qg;
    double QgMax;
    double QgMin;

    double shuntG;
    double shuntB;

    double x;
    double y;
};

struct Branch {
    int id;
    int fromBus;
    int toBus;

    double r;
    double x;
    double b;

    double tap;
    double phaseShift;
    double status;

    double rateA;
    double rateB;
    double rateC;

    double ratioMin;
    double ratioMax;
};

struct Generator {
    int id;
    int busId;
    double Pg;
    double Qg;
    double PgMax;
    double PgMin;
    double QgMax;
    double QgMin;
    double Vset;
    int status;
};

struct FlowResult {
    int fromBus;
    int toBus;
    double Pfrom;
    double Qfrom;
    double Pto;
    double Qto;
    double Ploss;
    double Qloss;
};

struct PowerFlowSolution {
    bool converged;
    int iterations;
    double tolerance;
    double elapsedMs;
    std::string errorMessage;

    std::vector<Bus> buses;
    std::vector<Branch> branches;
    std::vector<Generator> generators;
    std::vector<FlowResult> branchFlows;

    double totalLossesP;
    double totalLossesQ;
};

class GridData {
public:
    std::string caseName;
    std::string sourceFile;
    double baseMVA;

    std::vector<Bus> buses;
    std::vector<Branch> branches;
    std::vector<Generator> generators;

    std::map<int, int> busIdToIndex;

    int nBus() const { return static_cast<int>(buses.size()); }
    int nBranch() const { return static_cast<int>(branches.size()); }
    int nGen() const { return static_cast<int>(generators.size()); }

    int nPQ() const;
    int nPV() const;
    int nSlack() const;

    int findBusIndex(int busId) const;
    void buildBusIndexMap();
    void validate() const;
};

} // namespace GridSolver
