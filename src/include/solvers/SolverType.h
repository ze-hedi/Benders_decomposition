#pragma once

#include <map>
#include <ostream>
#include <string>

// MathOpt backends available in our OR-Tools build.
enum class SolverType
{
    GLOP,   // LP, linked
    PDLP,   // LP, linked
    HIGHS,  // LP + MIP, linked
    GSCIP,  // MIP, linked
    CP_SAT, // MIP, linked
    GUROBI, // LP + MIP, loaded at runtime (dlopen)
    XPRESS, // LP + MIP, loaded at runtime (dlopen)
};

// Solver of each name, in upper case
inline const std::map<std::string, SolverType> SOLVER_TYPE_BY_NAME = {
  {"GLOP", SolverType::GLOP},
  {"PDLP", SolverType::PDLP},
  {"HIGHS", SolverType::HIGHS},
  {"GSCIP", SolverType::GSCIP},
  {"CP_SAT", SolverType::CP_SAT},
  {"GUROBI", SolverType::GUROBI},
  {"XPRESS", SolverType::XPRESS},
};

// True when the solver handles integer variables, as a master that is not relaxed needs
inline bool SolvesMip(SolverType solver_type)
{
    return solver_type != SolverType::GLOP && solver_type != SolverType::PDLP;
}

// True when the solver gives the duals of an LP and its basis, as a subproblem needs for its cut
// and its warm start. PDLP is a first-order method: approximate duals and no basis
inline bool GivesDualsAndBasis(SolverType solver_type)
{
    return solver_type != SolverType::CP_SAT && solver_type != SolverType::PDLP;
}

inline std::ostream& operator<<(std::ostream& stream, const SolverType& solver_type)
{
    for (const auto& [name, type]: SOLVER_TYPE_BY_NAME)
    {
        if (type == solver_type)
        {
            return stream << name;
        }
    }
    return stream << "UNKNOWN";
}
