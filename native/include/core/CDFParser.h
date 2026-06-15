#pragma once
#include "core/GridData.h"
#include <string>
#include <stdexcept>
#include <istream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace GridSolver {

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& msg) : std::runtime_error(msg) {}
};

class CDFParser {
public:
    CDFParser();
    ~CDFParser();

    GridData parse(const std::string& filePath);
    GridData parseString(const std::string& content);

private:
    std::string trim(const std::string& s);

    void parseTitleData(GridData& grid, const std::string& line);
    void parseBusData(GridData& grid, std::istream& stream);
    void parseBranchData(GridData& grid, std::istream& stream);
};

} // namespace GridSolver
