#pragma once
#include "core/GridData.h"
#include <string>
#include <stdexcept>

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
    void skipLines(std::ifstream& file, int count);
    std::string trim(const std::string& s);

    void parseTitleData(GridData& grid, const std::string& line);
    void parseBusData(GridData& grid, std::ifstream& file);
    void parseBranchData(GridData& grid, std::ifstream& file);
};

} // namespace GridSolver
