#include "solvers/SolverUtils.h"

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ortools/math_opt/io/mps_converter.h"
#include "solvers/LinearSolvers.h"

namespace
{
constexpr double kInf = std::numeric_limits<double>::infinity();

// Xpress/CLP treat |v| >= 1e20 as infinite, and Benders relies on that (e.g. alpha's ub).
// SCIP's finite range is the open interval (-1e20, 1e20), so map the sentinel to a true inf.
constexpr double kSolverInfinity = 1e20;
} // namespace

namespace solver_utils
{
std::unique_ptr<math_opt::Model> read_prob_mps(const std::filesystem::path& filename)
{
    auto path = filename;
    if (path.extension() != ".mps")
    {
        path.replace_extension(".mps");
    }

    auto proto_or = operations_research::math_opt::ReadMpsFile(path.string());
    if (!proto_or.ok())
    {
        throw GenericSolverException("Failed to read MPS file: "
                                     + std::string(proto_or.status().message()));
    }

    auto model_or = math_opt::Model::FromModelProto(*proto_or);
    if (!model_or.ok())
    {
        throw GenericSolverException("Failed to build model from MPS: "
                                     + std::string(model_or.status().message()));
    }

    return std::move(*model_or);
}

math_opt::SolverType ToMathOptSolverType(SolverType solver_type)
{
    switch (solver_type)
    {
    case SolverType::GLOP:
        return math_opt::SolverType::kGlop;
    case SolverType::PDLP:
        return math_opt::SolverType::kPdlp;
    case SolverType::HIGHS:
        return math_opt::SolverType::kHighs;
    case SolverType::GSCIP:
        return math_opt::SolverType::kGscip;
    case SolverType::CP_SAT:
        return math_opt::SolverType::kCpSat;
    case SolverType::GUROBI:
        return math_opt::SolverType::kGurobi;
    case SolverType::XPRESS:
        return math_opt::SolverType::kXpress;
    }
    throw GenericSolverException("Unknown solver type");
}

double ToMathOptBound(double value)
{
    if (value >= kSolverInfinity)
    {
        return kInf;
    }
    if (value <= -kSolverInfinity)
    {
        return -kInf;
    }
    return value;
}

std::pair<double, double> row_type_to_bounds(char row_type, double rhs, double range)
{
    switch (row_type)
    {
    case 'L':
        return {-kInf, ToMathOptBound(rhs)};
    case 'G':
        return {ToMathOptBound(rhs), kInf};
    case 'E':
        return {ToMathOptBound(rhs), ToMathOptBound(rhs)};
    case 'R':
        return {ToMathOptBound(rhs - range), ToMathOptBound(rhs)};
    default:
    {
        std::stringstream buffer;
        buffer << "row_type_to_bounds: unknown row type '" << row_type << "'";
        throw GenericSolverException(buffer.str());
    }
    }
}

void check_range(int first, int last, int size, const char* caller)
{
    // An empty range (last == first - 1) is allowed, e.g. on a problem with no rows.
    if (first < 0 || last >= size || last < first - 1)
    {
        throw std::out_of_range(std::string(caller) + ": range [" + std::to_string(first) + ", "
                                + std::to_string(last) + "] outside [0, " + std::to_string(size - 1)
                                + "]");
    }
}

ConstraintsToAdd make_cut(const VariableMap& linking_columns,
                          const Point& subgradient,
                          const Point& x_cut,
                          double subproblem_cost,
                          const std::vector<int>& alpha_columns,
                          double tolerance)
{
    ConstraintsToAdd cut;
    cut.qrtype = {'L'};
    cut.mstart = {0};
    double rhs = -subproblem_cost;

    for (const auto& [name, column]: linking_columns)
    {
        const auto it = subgradient.find(name);
        if (it == subgradient.end())
        {
            continue;
        }
        cut.mclind.push_back(column);
        cut.dmatval.push_back(it->second);
        rhs += it->second * x_cut.at(name);
    }

    for (int alpha_column: alpha_columns)
    {
        cut.mclind.push_back(alpha_column);
        cut.dmatval.push_back(-1.0);
    }

    // Round the cut to clean it. The tolerance is expected to stay below 1, so the alpha
    // coefficients are never removed.
    const auto round = [tolerance](double value)
    { return std::abs(value) < tolerance ? 0.0 : value; };
    cut.rhs = {round(rhs)};
    for (auto& coefficient: cut.dmatval)
    {
        coefficient = round(coefficient);
    }
    return cut;
}
} // namespace solver_utils
