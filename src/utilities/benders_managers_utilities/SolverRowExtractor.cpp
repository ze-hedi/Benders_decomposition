#include "utilities/benders_managers_utilities/SolverRowExtractor.h"

#include <stdexcept>

SolverRowExtractor::SolverRowExtractor(std::shared_ptr<SolverMathOpt> solver):
    solver_(std::move(solver))
{
}

SolverRepresentedRows SolverRowExtractor::GetRow(const SolverMathOpt& solver, int row_pos)
{
    SolverRepresentedRows result;
    SparseMatrix row = solver.get_rows(row_pos, row_pos);
    result.mstart = {0, static_cast<int>(row.indices.size())};
    result.mclind = std::move(row.indices);
    result.dmatval = std::move(row.values);
    result.rhs = solver.get_rhs(row_pos, row_pos);
    result.qrtype_p = solver.get_row_type(row_pos, row_pos);
    result.range = solver.get_rhs_range(row_pos, row_pos);
    return result;
}

SolverRepresentedRows SolverRowExtractor::GetRow(const std::string& name)
{
    const int constraint_pos = solver_->get_row_index(name);
    if (constraint_pos < 0)
    {
        throw std::invalid_argument("SolverRowExtractor: no row named " + name);
    }
    auto result = GetRow(*solver_, constraint_pos);
    result.row_names = {name};
    return result;
}
