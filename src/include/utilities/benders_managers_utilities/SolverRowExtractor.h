#pragma once

#include <memory>
#include <string>
#include <vector>

#include "solvers/LinearSolvers.h"

/*!
 * \struct SolverRepresentedRows
 * \brief Rows in the layout expected by SolverMathOpt::add_rows
 *
 *  Unlike xpansion, the range of each row is kept so that R rows can be added back.
 */
struct SolverRepresentedRows
{
    std::vector<int> mstart;
    std::vector<int> mclind;
    std::vector<double> dmatval;
    std::vector<char> qrtype_p;
    std::vector<double> rhs;
    std::vector<double> range;
    std::vector<std::string> row_names;
};

/*!
 * \class SolverRowExtractor
 * \brief Reads single rows out of a solver, by name or position
 */
class SolverRowExtractor
{
public:
    explicit SolverRowExtractor(std::shared_ptr<SolverMathOpt> solver);

    /**
     * @throws std::invalid_argument if the solver has no row with this name
     */
    SolverRepresentedRows GetRow(const std::string& name);
    static SolverRepresentedRows GetRow(const SolverMathOpt& solver, int row_pos);

private:
    std::shared_ptr<SolverMathOpt> solver_;
};
