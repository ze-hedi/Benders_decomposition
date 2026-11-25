#include "solvers/LinearSolvers.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "ortools/math_opt/io/mps_converter.h"

namespace
{
constexpr double kInf = std::numeric_limits<double>::infinity();

// Xpress/CLP treat |v| >= 1e20 as infinite, and Benders relies on that (e.g. alpha's ub).
// SCIP's finite range is the open interval (-1e20, 1e20), so map the sentinel to a true inf.
constexpr double kSolverInfinity = 1e20;

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

// Checks the sparse part of add_rows/add_cols: mstart has one offset per new row (or column),
// optionally followed by the number of entries, offsets are increasing and within the entries,
// and every index refers to an existing column (or row).
void check_sparse_input(const char* caller,
                        size_t count,
                        int nnz,
                        const std::vector<int>& mstart,
                        const std::vector<int>& indices,
                        int index_bound)
{
    if (static_cast<int>(indices.size()) != nnz)
    {
        throw std::invalid_argument(std::string(caller)
                                    + ": indices and values must have the same size");
    }
    if (mstart.empty())
    {
        if (nnz != 0 && count != 0)
        {
            throw std::invalid_argument(std::string(caller) + ": mstart is empty but there are "
                                                              "matrix entries");
        }
    }
    else
    {
        if (mstart.size() != count && mstart.size() != count + 1)
        {
            throw std::invalid_argument(std::string(caller)
                                        + ": mstart must have one offset per row/column, plus an "
                                          "optional last one");
        }
        int previous = 0;
        for (int start: mstart)
        {
            if (start < previous || start > nnz)
            {
                throw std::invalid_argument(std::string(caller)
                                            + ": mstart offsets must increase and stay within "
                                              "the entries");
            }
            previous = start;
        }
    }
    for (int index: indices)
    {
        if (index < 0 || index >= index_bound)
        {
            throw std::out_of_range(std::string(caller) + ": index " + std::to_string(index)
                                    + " outside the problem");
        }
    }
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

std::string SolverTypeToName(SolverType solver_type)
{
    switch (solver_type)
    {
    case SolverType::GLOP:
        return "GLOP";
    case SolverType::PDLP:
        return "PDLP";
    case SolverType::HIGHS:
        return "HIGHS";
    case SolverType::GSCIP:
        return "GSCIP";
    case SolverType::CP_SAT:
        return "CP_SAT";
    case SolverType::GUROBI:
        return "GUROBI";
    case SolverType::XPRESS:
        return "XPRESS";
    }
    throw GenericSolverException("Unknown solver type");
}
} // namespace

SolverType SolverTypeFromString(const std::string& solver_name)
{
    std::string upper_name(solver_name);
    std::ranges::transform(upper_name,
                           upper_name.begin(),
                           [](unsigned char c) { return std::toupper(c); });
    for (auto solver_type: {SolverType::GLOP,
                            SolverType::PDLP,
                            SolverType::HIGHS,
                            SolverType::GSCIP,
                            SolverType::CP_SAT,
                            SolverType::GUROBI,
                            SolverType::XPRESS})
    {
        if (SolverTypeToName(solver_type) == upper_name)
        {
            return solver_type;
        }
    }
    throw GenericSolverException("Unknown solver name '" + solver_name + "'");
}

/*************************************************************************************************
-----------------------------------    Constructor/Destructor
--------------------------------
*************************************************************************************************/

SolverMathOpt::SolverMathOpt(const std::filesystem::path& mps_path,
                             const std::filesystem::path& log_file,
                             SolverType solver_type):
    name_(SolverTypeToName(solver_type)),
    _solver_type(solver_type),
    _model(std::make_unique<math_opt::Model>("mathopt"))
{
    if (!log_file.empty())
    {
        _log_file = log_file;
        _log_stream.open(_log_file, std::ofstream::out | std::ofstream::app);
        add_stream(_log_stream);
    }
    read_prob_mps(mps_path);
}

SolverMathOpt::SolverMathOpt(const SolverMathOpt& other):
    name_(other.name_),
    _solver_type(other._solver_type)
{
    _log_file = other._log_file;
    if (_log_file != "")
    {
        _log_stream.open(_log_file, std::ofstream::out | std::ofstream::app);
        add_stream(_log_stream);
    }
    _minimize = other._minimize;
    _log_level = other._log_level;
    args_.parameters = other.args_.parameters;

    // Rebuild model from scratch by copying structure
    _model = std::make_unique<math_opt::Model>("mathopt");

    // Copy variables
    for (const auto& var: other._variables)
    {
        auto new_var = _model->AddContinuousVariable(other._model->lower_bound(var),
                                                     other._model->upper_bound(var),
                                                     other._model->name(var));
        if (other._model->is_integer(var))
        {
            _model->set_integer(new_var);
        }
        _variables.push_back(new_var);
    }

    // Copy objective
    if (other._minimize)
    {
        _model->set_minimize();
    }
    else
    {
        _model->set_maximize();
    }
    for (size_t i = 0; i < other._variables.size(); ++i)
    {
        _model->set_objective_coefficient(_variables[i],
                                          other._model->objective_coefficient(other._variables[i]));
    }

    // Copy constraints
    for (size_t r = 0; r < other._constraints.size(); ++r)
    {
        const auto& src_ct = other._constraints[r];
        double lb = other._model->lower_bound(src_ct);
        double ub = other._model->upper_bound(src_ct);

        auto new_ct = _model->AddLinearConstraint(lb, ub, other._model->name(src_ct));

        // Copy coefficients
        for (const auto& var: other._model->RowNonzeros(src_ct))
        {
            double coeff = other._model->coefficient(src_ct, var);
            // Find the index of this variable in other._variables
            for (size_t j = 0; j < other._variables.size(); ++j)
            {
                if (var.id() == other._variables[j].id())
                {
                    _model->set_coefficient(new_ct, _variables[j], coeff);
                    break;
                }
            }
        }
        _constraints.push_back(new_ct);
    }

    _row_types = other._row_types;
    rebuild_index_maps();
}

SolverMathOpt::~SolverMathOpt()
{
    if (_log_stream.is_open())
    {
        _log_stream.close();
    }
}

SolverMathOpt* SolverMathOpt::clone() const
{
    return new SolverMathOpt(*this);
}

/*************************************************************************************************
-----------------------------------    Init / Free
--------------------------------
*************************************************************************************************/

void SolverMathOpt::init()
{
    _model = std::make_unique<math_opt::Model>("mathopt");
    _variables.clear();
    _constraints.clear();
    _row_types.clear();
    _last_result.reset();
    _initial_basis.reset();
    _col_name_to_index.clear();
    _row_name_to_index.clear();
    _minimize = true;
}

void SolverMathOpt::free()
{
    // RAII — nothing to do
}

/*************************************************************************************************
-------------------------------    Reading & Writing problems
-------------------------------
*************************************************************************************************/

void SolverMathOpt::write_prob_mps(const std::filesystem::path& filename)
{
    auto proto = _model->ExportModel();
    auto mps_or = operations_research::math_opt::ModelProtoToMps(proto);
    if (!mps_or.ok())
    {
        throw GenericSolverException("Failed to export MPS: "
                                     + std::string(mps_or.status().message()));
    }

    auto path = filename;
    if (path.extension() != ".mps")
    {
        path.replace_extension(".mps");
    }
    std::ofstream ofs(path);
    ofs << *mps_or;
}

void SolverMathOpt::save_prob(const std::filesystem::path& filename)
{
    write_prob_mps(filename);
}

void SolverMathOpt::write_basis(const std::filesystem::path& filename)
{
    // MIP solves return no basis in MathOpt. The basis file is only an optional warm start
    // (sensitivity analysis), so warn and skip instead of aborting the run.
    if (!_last_result.has_value() || _last_result->solutions.empty()
        || !_last_result->solutions[0].basis.has_value())
    {
        for (auto* stream: get_stream())
        {
            *stream << "Warning: write_basis: no basis available (solve not called or MIP solve), "
                    << filename.string() << " not written" << std::endl;
        }
        return;
    }

    const auto& basis = *_last_result->solutions[0].basis;
    using BS = math_opt::BasisStatus;

    auto status_to_int = [](BS s) -> int
    {
        switch (s)
        {
        case BS::kFree:
            return 0;
        case BS::kBasic:
            return 1;
        case BS::kAtUpperBound:
            return 2;
        case BS::kAtLowerBound:
            return 3;
        case BS::kFixedValue:
            return 4;
        default:
            return 0;
        }
    };

    std::ofstream ofs(filename);
    if (!ofs)
    {
        throw GenericSolverException("write_basis: cannot open file " + filename.string());
    }

    ofs << "MATHOPT_BASIS v1\n";
    ofs << "COLUMNS " << get_ncols() << "\n";
    for (int i = 0; i < get_ncols(); ++i)
    {
        auto it = basis.variable_status.find(_variables[i]);
        int s = (it != basis.variable_status.end()) ? status_to_int(it->second) : 0;
        ofs << std::string(_model->name(_variables[i])) << " " << s << "\n";
    }

    ofs << "ROWS " << get_nrows() << "\n";
    for (int i = 0; i < get_nrows(); ++i)
    {
        auto it = basis.constraint_status.find(_constraints[i]);
        int s = (it != basis.constraint_status.end()) ? status_to_int(it->second) : 0;
        ofs << std::string(_model->name(_constraints[i])) << " " << s << "\n";
    }

    ofs << "ENDATA\n";
}

void SolverMathOpt::read_prob_mps(const std::filesystem::path& filename)
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

    _model = std::move(*model_or);
    rebuild_from_model();
}

void SolverMathOpt::restore_prob(const std::filesystem::path& filename)
{
    read_prob_mps(filename);
}

void SolverMathOpt::read_basis(const std::filesystem::path& filename)
{
    std::ifstream ifs(filename);
    if (!ifs)
    {
        throw GenericSolverException("read_basis: cannot open file " + filename.string());
    }

    auto int_to_status = [](int s) -> math_opt::BasisStatus
    {
        switch (s)
        {
        case 1:
            return math_opt::BasisStatus::kBasic;
        case 2:
            return math_opt::BasisStatus::kAtUpperBound;
        case 3:
            return math_opt::BasisStatus::kAtLowerBound;
        case 4:
            return math_opt::BasisStatus::kFixedValue;
        default:
            return math_opt::BasisStatus::kFree;
        }
    };

    std::string line;
    std::getline(ifs, line); // "MATHOPT_BASIS v1"

    math_opt::Basis basis;

    // Read column statuses
    int ncols = 0;
    ifs >> line >> ncols;    // "COLUMNS <n>"
    std::getline(ifs, line); // consume newline
    for (int i = 0; i < ncols && i < get_ncols(); ++i)
    {
        std::string name;
        int status;
        ifs >> name >> status;
        basis.variable_status[_variables[i]] = int_to_status(status);
    }

    // Read row statuses
    int nrows = 0;
    ifs >> line >> nrows;    // "ROWS <n>"
    std::getline(ifs, line); // consume newline
    for (int i = 0; i < nrows && i < get_nrows(); ++i)
    {
        std::string name;
        int status;
        ifs >> name >> status;
        basis.constraint_status[_constraints[i]] = int_to_status(status);
    }

    _initial_basis = std::move(basis);
}

void SolverMathOpt::set_basis(const std::vector<int>& rstatus, const std::vector<int>& cstatus)
{
    auto int_to_status = [](int s) -> math_opt::BasisStatus
    {
        switch (s)
        {
        case 1:
            return math_opt::BasisStatus::kBasic;
        case 2:
            return math_opt::BasisStatus::kAtUpperBound;
        case 3:
            return math_opt::BasisStatus::kAtLowerBound;
        case 4:
            return math_opt::BasisStatus::kFixedValue;
        default:
            return math_opt::BasisStatus::kFree;
        }
    };

    math_opt::Basis basis;
    for (size_t i = 0; i < cstatus.size() && i < _variables.size(); ++i)
    {
        basis.variable_status[_variables[i]] = int_to_status(cstatus[i]);
    }
    for (size_t i = 0; i < rstatus.size() && i < _constraints.size(); ++i)
    {
        basis.constraint_status[_constraints[i]] = int_to_status(rstatus[i]);
    }

    _initial_basis = std::move(basis);
}

/*************************************************************************************************
-----------------------    Get general informations about problem
----------------------------
*************************************************************************************************/

int SolverMathOpt::get_ncols() const
{
    return static_cast<int>(_variables.size());
}

int SolverMathOpt::get_nrows() const
{
    return static_cast<int>(_constraints.size());
}

int SolverMathOpt::get_nelems() const
{
    int count = 0;
    for (const auto& ct: _constraints)
    {
        count += static_cast<int>(_model->RowNonzeros(ct).size());
    }
    return count;
}

int SolverMathOpt::get_n_integer_vars() const
{
    int count = 0;
    for (const auto& var: _variables)
    {
        if (_model->is_integer(var))
        {
            ++count;
        }
    }
    return count;
}

std::vector<double> SolverMathOpt::get_obj(int first, int last) const
{
    check_range(first, last, get_ncols(), "get_obj");
    std::vector<double> obj;
    obj.reserve(last - first + 1);
    for (int i = first; i <= last; ++i)
    {
        obj.push_back(_model->objective_coefficient(_variables[i]));
    }
    return obj;
}

std::vector<double> SolverMathOpt::get_obj() const
{
    return get_obj(0, get_ncols() - 1);
}

void SolverMathOpt::set_obj_to_zero()
{
    for (const auto& var: _variables)
    {
        _model->set_objective_coefficient(var, 0.0);
    }
}

void SolverMathOpt::set_obj(const std::vector<double>& obj, int first)
{
    check_range(first, first + static_cast<int>(obj.size()) - 1, get_ncols(), "set_obj");
    for (size_t k = 0; k < obj.size(); ++k)
    {
        _model->set_objective_coefficient(_variables[first + k], obj[k]);
    }
}

SparseMatrix SolverMathOpt::get_rows(int first, int last) const
{
    check_range(first, last, get_nrows(), "get_rows");
    std::unordered_map<int64_t, int> column_of_variable;
    for (int j = 0; j < get_ncols(); ++j)
    {
        column_of_variable[_variables[j].id()] = j;
    }

    SparseMatrix rows;
    for (int r = first; r <= last; ++r)
    {
        rows.starts.push_back(static_cast<int>(rows.indices.size()));
        for (const auto& var: _model->RowNonzeros(_constraints[r]))
        {
            rows.indices.push_back(column_of_variable.at(var.id()));
            rows.values.push_back(_model->coefficient(_constraints[r], var));
        }
    }
    rows.starts.push_back(static_cast<int>(rows.indices.size()));
    return rows;
}

std::vector<char> SolverMathOpt::get_row_type(int first, int last) const
{
    check_range(first, last, get_nrows(), "get_row_type");
    return {_row_types.begin() + first, _row_types.begin() + last + 1};
}

std::vector<char> SolverMathOpt::get_row_type() const
{
    return get_row_type(0, get_nrows() - 1);
}

std::vector<double> SolverMathOpt::get_rhs(int first, int last) const
{
    check_range(first, last, get_nrows(), "get_rhs");
    std::vector<double> rhs;
    rhs.reserve(last - first + 1);
    for (int i = first; i <= last; ++i)
    {
        switch (_row_types[i])
        {
        case 'E':
        case 'G':
            rhs.push_back(_model->lower_bound(_constraints[i]));
            break;
        case 'L':
        case 'R':
            rhs.push_back(_model->upper_bound(_constraints[i]));
            break;
        default:
            rhs.push_back(0.0);
        }
    }
    return rhs;
}

std::vector<double> SolverMathOpt::get_rhs() const
{
    return get_rhs(0, get_nrows() - 1);
}

std::vector<double> SolverMathOpt::get_rhs_range(int first, int last) const
{
    check_range(first, last, get_nrows(), "get_rhs_range");
    std::vector<double> range;
    range.reserve(last - first + 1);
    for (int i = first; i <= last; ++i)
    {
        if (_row_types[i] == 'R')
        {
            range.push_back(_model->upper_bound(_constraints[i])
                            - _model->lower_bound(_constraints[i]));
        }
        else
        {
            range.push_back(0.0);
        }
    }
    return range;
}

std::vector<double> SolverMathOpt::get_rhs_range() const
{
    return get_rhs_range(0, get_nrows() - 1);
}

SparseMatrix SolverMathOpt::get_cols(int first, int last) const
{
    check_range(first, last, get_ncols(), "get_cols");
    std::unordered_map<int64_t, int> row_of_constraint;
    for (int r = 0; r < get_nrows(); ++r)
    {
        row_of_constraint[_constraints[r].id()] = r;
    }

    SparseMatrix cols;
    for (int c = first; c <= last; ++c)
    {
        cols.starts.push_back(static_cast<int>(cols.indices.size()));
        for (const auto& ct: _model->ColumnNonzeros(_variables[c]))
        {
            cols.indices.push_back(row_of_constraint.at(ct.id()));
            cols.values.push_back(_model->coefficient(ct, _variables[c]));
        }
    }
    cols.starts.push_back(static_cast<int>(cols.indices.size()));
    return cols;
}

std::vector<char> SolverMathOpt::get_col_type(int first, int last) const
{
    check_range(first, last, get_ncols(), "get_col_type");
    std::vector<char> coltype;
    coltype.reserve(last - first + 1);
    for (int i = first; i <= last; ++i)
    {
        if (!_model->is_integer(_variables[i]))
        {
            coltype.push_back('C');
        }
        else if (_model->lower_bound(_variables[i]) == 0.0
                 && _model->upper_bound(_variables[i]) == 1.0)
        {
            coltype.push_back('B');
        }
        else
        {
            coltype.push_back('I');
        }
    }
    return coltype;
}

std::vector<char> SolverMathOpt::get_col_type() const
{
    return get_col_type(0, get_ncols() - 1);
}

std::vector<double> SolverMathOpt::get_lb(int first, int last) const
{
    check_range(first, last, get_ncols(), "get_lb");
    std::vector<double> lb;
    lb.reserve(last - first + 1);
    for (int i = first; i <= last; ++i)
    {
        lb.push_back(_model->lower_bound(_variables[i]));
    }
    return lb;
}

std::vector<double> SolverMathOpt::get_lb() const
{
    return get_lb(0, get_ncols() - 1);
}

std::vector<double> SolverMathOpt::get_ub(int first, int last) const
{
    check_range(first, last, get_ncols(), "get_ub");
    std::vector<double> ub;
    ub.reserve(last - first + 1);
    for (int i = first; i <= last; ++i)
    {
        ub.push_back(_model->upper_bound(_variables[i]));
    }
    return ub;
}

std::vector<double> SolverMathOpt::get_ub() const
{
    return get_ub(0, get_ncols() - 1);
}

int SolverMathOpt::get_row_index(const std::string& name)
{
    auto it = _row_name_to_index.find(name);
    return (it != _row_name_to_index.end()) ? it->second : -1;
}

int SolverMathOpt::get_col_index(const std::string& name)
{
    auto it = _col_name_to_index.find(name);
    return (it != _col_name_to_index.end()) ? it->second : -1;
}

std::vector<std::string> SolverMathOpt::get_row_names(int first, int last) const
{
    std::vector<std::string> names;
    names.reserve(last - first + 1);
    for (int i = first; i <= last; ++i)
    {
        names.emplace_back(_model->name(_constraints[i]));
    }
    return names;
}

std::vector<std::string> SolverMathOpt::get_row_names()
{
    return get_row_names(0, get_nrows() - 1);
}

std::vector<std::string> SolverMathOpt::get_col_names(int first, int last) const
{
    std::vector<std::string> names;
    names.reserve(last - first + 1);
    for (int i = first; i <= last; ++i)
    {
        names.emplace_back(_model->name(_variables[i]));
    }
    return names;
}

std::vector<std::string> SolverMathOpt::get_col_names()
{
    return get_col_names(0, get_ncols() - 1);
}

/*************************************************************************************************
------------------------------    Methods to modify problem
----------------------------------
*************************************************************************************************/

void SolverMathOpt::del_rows(int first, int last)
{
    for (int i = last; i >= first; --i)
    {
        _model->DeleteLinearConstraint(_constraints[i]);
    }
    _constraints.erase(_constraints.begin() + first, _constraints.begin() + last + 1);
    _row_types.erase(_row_types.begin() + first, _row_types.begin() + last + 1);
    rebuild_index_maps();
}

void SolverMathOpt::del_cols(int first, int last)
{
    for (int i = last; i >= first; --i)
    {
        _model->DeleteVariable(_variables[i]);
    }
    _variables.erase(_variables.begin() + first, _variables.begin() + last + 1);
    rebuild_index_maps();
}

void SolverMathOpt::add_rows(const std::vector<char>& qrtype,
                             const std::vector<double>& rhs,
                             const std::vector<double>& range,
                             const std::vector<int>& mstart,
                             const std::vector<int>& mclind,
                             const std::vector<double>& dmatval,
                             const std::vector<std::string>& row_names)
{
    const size_t newrows = qrtype.size();
    const int newnz = static_cast<int>(dmatval.size());
    check_sparse_input("add_rows", newrows, newnz, mstart, mclind, get_ncols());
    if (rhs.size() != newrows || (!range.empty() && range.size() != newrows)
        || (!row_names.empty() && row_names.size() != newrows))
    {
        throw std::invalid_argument("add_rows: rhs, range and row_names must be empty or have "
                                    "one entry per row");
    }

    for (size_t i = 0; i < newrows; ++i)
    {
        // Determine bounds from row type
        double lb, ub;
        switch (qrtype[i])
        {
        case 'L':
            lb = -kInf;
            ub = ToMathOptBound(rhs[i]);
            break;
        case 'G':
            lb = ToMathOptBound(rhs[i]);
            ub = kInf;
            break;
        case 'E':
            lb = ToMathOptBound(rhs[i]);
            ub = ToMathOptBound(rhs[i]);
            break;
        case 'R':
            ub = ToMathOptBound(rhs[i]);
            lb = ToMathOptBound(rhs[i] - (range.empty() ? 0.0 : range[i]));
            break;
        default:
        {
            std::stringstream buffer;
            buffer << "add_rows: unknown row type '" << qrtype[i] << "' for row " << i;
            throw GenericSolverException(buffer.str());
        }
        }

        const std::string name = row_names.empty() ? "" : row_names[i];
        auto ct = _model->AddLinearConstraint(lb, ub, name);

        const int start = mstart.empty() ? 0 : mstart[i];
        const int end = mstart.empty() ? 0 : (i + 1 < newrows) ? mstart[i + 1] : newnz;
        for (int k = start; k < end; ++k)
        {
            _model->set_coefficient(ct, _variables[mclind[k]], dmatval[k]);
        }

        _constraints.push_back(ct);
        _row_types.push_back(qrtype[i]);

        if (!name.empty())
        {
            _row_name_to_index[name] = static_cast<int>(_constraints.size()) - 1;
        }
    }
}

void SolverMathOpt::add_cols(const std::vector<double>& obj,
                             const std::vector<int>& mstart,
                             const std::vector<int>& mrwind,
                             const std::vector<double>& dmatval,
                             const std::vector<double>& lb,
                             const std::vector<double>& ub,
                             const std::vector<char>& col_types,
                             const std::vector<std::string>& col_names)
{
    const size_t newcols = obj.size();
    const int newnz = static_cast<int>(dmatval.size());
    check_sparse_input("add_cols", newcols, newnz, mstart, mrwind, get_nrows());
    if (lb.size() != newcols || ub.size() != newcols
        || (!col_types.empty() && col_types.size() != newcols)
        || (!col_names.empty() && col_names.size() != newcols))
    {
        throw std::invalid_argument("add_cols: lb, ub, col_types and col_names must be empty or "
                                    "have one entry per column");
    }

    for (size_t i = 0; i < newcols; ++i)
    {
        const char type = col_types.empty() ? 'C' : col_types[i];
        double lower = ToMathOptBound(lb[i]);
        double upper = ToMathOptBound(ub[i]);
        switch (type)
        {
        case 'C':
        case 'I':
            break;
        case 'B':
            lower = 0.0;
            upper = 1.0;
            break;
        default:
            throw InvalidColTypeException(type);
        }

        const std::string name = col_names.empty() ? "" : col_names[i];
        auto var = _model->AddVariable(lower, upper, type != 'C', name);
        _model->set_objective_coefficient(var, obj[i]);

        // Set column coefficients in existing constraints
        const int start = mstart.empty() ? 0 : mstart[i];
        const int end = mstart.empty() ? 0 : (i + 1 < newcols) ? mstart[i + 1] : newnz;
        for (int k = start; k < end; ++k)
        {
            _model->set_coefficient(_constraints[mrwind[k]], var, dmatval[k]);
        }

        _variables.push_back(var);
        if (!name.empty())
        {
            _col_name_to_index[name] = static_cast<int>(_variables.size()) - 1;
        }
    }
}

void SolverMathOpt::chg_obj(const std::vector<int>& mindex, const std::vector<double>& obj)
{
    assert(obj.size() == mindex.size());
    for (size_t i = 0; i < mindex.size(); ++i)
    {
        _model->set_objective_coefficient(_variables[mindex[i]], obj[i]);
    }
}

void SolverMathOpt::chg_obj_direction(bool minimize)
{
    _minimize = minimize;
    if (minimize)
    {
        _model->set_minimize();
    }
    else
    {
        _model->set_maximize();
    }
}

void SolverMathOpt::chg_bounds(const std::vector<int>& mindex,
                               const std::vector<char>& qbtype,
                               const std::vector<double>& bnd)
{
    assert(qbtype.size() == mindex.size());
    assert(bnd.size() == mindex.size());
    for (size_t i = 0; i < mindex.size(); ++i)
    {
        auto& var = _variables[mindex[i]];
        const double value = ToMathOptBound(bnd[i]);
        switch (qbtype[i])
        {
        case 'L':
            _model->set_lower_bound(var, value);
            break;
        case 'U':
            _model->set_upper_bound(var, value);
            break;
        case 'B':
            _model->set_lower_bound(var, value);
            _model->set_upper_bound(var, value);
            break;
        default:
            throw InvalidBoundTypeException(qbtype[i]);
        }
    }
}

void SolverMathOpt::chg_col_type(const std::vector<int>& mindex, const std::vector<char>& qctype)
{
    assert(qctype.size() == mindex.size());
    for (size_t i = 0; i < mindex.size(); ++i)
    {
        auto& var = _variables[mindex[i]];
        switch (qctype[i])
        {
        case 'C':
            _model->set_continuous(var);
            break;
        case 'I':
            _model->set_integer(var);
            break;
        case 'B':
            _model->set_integer(var);
            _model->set_lower_bound(var, 0.0);
            _model->set_upper_bound(var, 1.0);
            break;
        default:
            throw InvalidColTypeException(qctype[i]);
        }
    }
}

void SolverMathOpt::chg_rhs(int id_row, double val)
{
    val = ToMathOptBound(val);
    char type = _row_types[id_row];
    auto& ct = _constraints[id_row];

    switch (type)
    {
    case 'E':
        _model->set_lower_bound(ct, val);
        _model->set_upper_bound(ct, val);
        break;
    case 'L':
        _model->set_upper_bound(ct, val);
        break;
    case 'G':
        _model->set_lower_bound(ct, val);
        break;
    case 'R':
    {
        double old_ub = _model->upper_bound(ct);
        double old_lb = _model->lower_bound(ct);
        double range = old_ub - old_lb;
        _model->set_upper_bound(ct, val);
        _model->set_lower_bound(ct, val - range);
        break;
    }
    default:
    {
        std::stringstream buffer;
        buffer << "chg_rhs: unconstrained constraint " << id_row;
        throw GenericSolverException(buffer.str());
    }
    }
}

void SolverMathOpt::chg_coef(int id_row, int id_col, double val)
{
    _model->set_coefficient(_constraints[id_row], _variables[id_col], val);
}

void SolverMathOpt::chg_coefs(const std::vector<int>& id_rows,
                              const std::vector<int>& id_cols,
                              const std::vector<double>& vals)
{
    for (size_t i = 0; i < id_rows.size(); ++i)
    {
        chg_coef(id_rows[i], id_cols[i], vals[i]);
    }
}

void SolverMathOpt::chg_rhs_values(const std::vector<int>& rhs_indices,
                                   const std::vector<double>& rhs_values)
{
    for (size_t i = 0; i < rhs_indices.size(); ++i)
    {
        chg_rhs(rhs_indices[i], rhs_values[i]);
    }
}

void SolverMathOpt::chg_row_name(int id_row, const std::string& name)
{
    // Remove old name from map
    std::string old_name(_model->name(_constraints[id_row]));
    if (!old_name.empty())
    {
        _row_name_to_index.erase(old_name);
    }

    // MathOpt doesn't support renaming after creation.
    // We delete and re-create the constraint with the new name.
    auto& ct = _constraints[id_row];
    double lb = _model->lower_bound(ct);
    double ub = _model->upper_bound(ct);

    // Save coefficients
    std::vector<std::pair<math_opt::Variable, double>> coeffs;
    for (const auto& var: _model->RowNonzeros(ct))
    {
        coeffs.emplace_back(var, _model->coefficient(ct, var));
    }

    _model->DeleteLinearConstraint(ct);
    auto new_ct = _model->AddLinearConstraint(lb, ub, name);
    for (const auto& [var, coeff]: coeffs)
    {
        _model->set_coefficient(new_ct, var, coeff);
    }
    _constraints[id_row] = new_ct;

    if (!name.empty())
    {
        _row_name_to_index[name] = id_row;
    }
}

void SolverMathOpt::chg_col_name(int id_col, const std::string& name)
{
    // Remove old name from map
    std::string old_name(_model->name(_variables[id_col]));
    if (!old_name.empty())
    {
        _col_name_to_index.erase(old_name);
    }

    // MathOpt doesn't support renaming after creation.
    // We delete and re-create the variable with the new name.
    auto& var = _variables[id_col];
    double lb = _model->lower_bound(var);
    double ub = _model->upper_bound(var);
    bool is_int = _model->is_integer(var);
    double obj_coeff = _model->objective_coefficient(var);

    // Save column coefficients
    std::vector<std::pair<math_opt::LinearConstraint, double>> coeffs;
    for (const auto& ct: _model->ColumnNonzeros(var))
    {
        coeffs.emplace_back(ct, _model->coefficient(ct, var));
    }

    _model->DeleteVariable(var);
    math_opt::Variable new_var = is_int ? _model->AddIntegerVariable(lb, ub, name)
                                        : _model->AddContinuousVariable(lb, ub, name);
    _model->set_objective_coefficient(new_var, obj_coeff);

    for (const auto& [ct, coeff]: coeffs)
    {
        _model->set_coefficient(ct, new_var, coeff);
    }
    _variables[id_col] = new_var;

    if (!name.empty())
    {
        _col_name_to_index[name] = id_col;
    }
}

/*************************************************************************************************
-----------------------------    Methods to solve the problem
---------------------------------
*************************************************************************************************/

int SolverMathOpt::solve_lp()
{
    if (_log_level > 0 && _log_stream.is_open())
    {
        args_.message_callback = math_opt::PrinterMessageCallback(_log_stream);
    }
    else
    {
        args_.parameters.enable_output = (_log_level > 0);
    }

    // Pass warm-start basis if one was set via set_basis() or read_basis().
    // Consumed on use so subsequent solves start fresh unless re-set: args_ is kept between
    // solves, so it is always overwritten, otherwise a stale basis would be passed again
    args_.model_parameters.initial_basis = std::exchange(_initial_basis, std::nullopt);

    auto result = math_opt::Solve(*_model, ToMathOptSolverType(_solver_type), args_);
    if (!result.ok())
    {
        std::stringstream buffer;
        buffer << "MathOpt solve failed: " << result.status();
        for (auto* stream: get_stream())
        {
            *stream << buffer.str() << std::endl;
        }
        return UNKNOWN;
    }

    _last_result = std::move(*result);

    auto reason = _last_result->termination.reason;
    using TR = math_opt::TerminationReason;

    if (reason == TR::kOptimal)
    {
        return OPTIMAL;
    }
    else if (reason == TR::kInfeasible)
    {
        return INFEASIBLE;
    }
    else if (reason == TR::kUnbounded || reason == TR::kInfeasibleOrUnbounded)
    {
        return UNBOUNDED;
    }
    else
    {
        return UNKNOWN;
    }
}

int SolverMathOpt::solve_mip()
{
    if (_log_level > 0 && _log_stream.is_open())
    {
        args_.message_callback = math_opt::PrinterMessageCallback(_log_stream);
    }
    else
    {
        args_.parameters.enable_output = (_log_level > 0);
    }

    // A basis only warm starts an LP solve, drop the one a previous LP solve may have left
    args_.model_parameters.initial_basis.reset();

    auto result = math_opt::Solve(*_model, ToMathOptSolverType(_solver_type), args_);
    if (!result.ok())
    {
        std::stringstream buffer;
        buffer << "MathOpt MIP solve failed: " << result.status();
        for (auto* stream: get_stream())
        {
            *stream << buffer.str() << std::endl;
        }
        return UNKNOWN;
    }

    _last_result = std::move(*result);

    auto reason = _last_result->termination.reason;
    using TR = math_opt::TerminationReason;

    if (reason == TR::kOptimal)
    {
        return OPTIMAL;
    }
    else if (reason == TR::kInfeasible)
    {
        return INFEASIBLE;
    }
    else if (reason == TR::kUnbounded || reason == TR::kInfeasibleOrUnbounded)
    {
        return UNBOUNDED;
    }
    else
    {
        return UNKNOWN;
    }
}

/*************************************************************************************************
-------------------------    Methods to get solutions information
-----------------------------
*************************************************************************************************/

BasisStatuses SolverMathOpt::get_basis() const
{
    BasisStatuses statuses{std::vector<int>(get_nrows(), 0), std::vector<int>(get_ncols(), 0)};
    if (!_last_result.has_value() || _last_result->solutions.empty()
        || !_last_result->solutions[0].basis.has_value())
    {
        // No basis available — all zeros
        return statuses;
    }

    const auto& basis = *_last_result->solutions[0].basis;
    using BS = math_opt::BasisStatus;

    auto map_status = [](BS s) -> int
    {
        switch (s)
        {
        case BS::kFree:
            return 0;
        case BS::kBasic:
            return 1;
        case BS::kAtUpperBound:
            return 2;
        case BS::kAtLowerBound:
            return 3;
        case BS::kFixedValue:
            return 4;
        default:
            return 0;
        }
    };

    for (int i = 0; i < get_ncols(); ++i)
    {
        auto it = basis.variable_status.find(_variables[i]);
        if (it != basis.variable_status.end())
        {
            statuses.cols[i] = map_status(it->second);
        }
    }
    for (int i = 0; i < get_nrows(); ++i)
    {
        auto it = basis.constraint_status.find(_constraints[i]);
        if (it != basis.constraint_status.end())
        {
            statuses.rows[i] = map_status(it->second);
        }
    }
    return statuses;
}

double SolverMathOpt::get_mip_value() const
{
    if (!_last_result.has_value())
    {
        throw GenericSolverException("get_mip_value: no solution available (solve not called)");
    }
    // Use primal_bound instead of objective_value() because objective_value()
    // CHECK-fails when no primal feasible solution exists (e.g. INFEASIBLE).
    // primal_bound is always safe and equals objective_value() on OPTIMAL.
    return _last_result->termination.objective_bounds.primal_bound;
}

double SolverMathOpt::get_lp_value() const
{
    if (!_last_result.has_value())
    {
        throw GenericSolverException("get_lp_value: no solution available (solve not called)");
    }
    // Use primal_bound instead of objective_value() because objective_value()
    // CHECK-fails when no primal feasible solution exists (e.g. INFEASIBLE).
    // primal_bound is always safe and equals objective_value() on OPTIMAL.
    return _last_result->termination.objective_bounds.primal_bound;
}

int SolverMathOpt::get_splex_num_of_ite_last() const
{
    if (!_last_result.has_value())
    {
        return 0;
    }
    return static_cast<int>(_last_result->solve_stats.simplex_iterations);
}

std::vector<double> SolverMathOpt::get_lp_primal() const
{
    return primal_values("get_lp_primal");
}

std::vector<double> SolverMathOpt::get_lp_duals() const
{
    if (!_last_result.has_value())
    {
        throw GenericSolverException("get_lp_duals: no solution available");
    }

    std::vector<double> duals(get_nrows(), 0.0);
    // Without a dual feasible solution (e.g. after a MIP solve), the duals stay at 0.
    if (_last_result->has_dual_feasible_solution())
    {
        const auto& dvals = _last_result->dual_values();
        for (int i = 0; i < get_nrows(); ++i)
        {
            auto it = dvals.find(_constraints[i]);
            if (it != dvals.end())
            {
                duals[i] = it->second;
            }
        }
    }
    return duals;
}

std::vector<double> SolverMathOpt::get_lp_reduced_costs() const
{
    if (!_last_result.has_value())
    {
        throw GenericSolverException("get_lp_reduced_costs: no solution available");
    }

    std::vector<double> reduced_costs(get_ncols(), 0.0);
    // Without a dual feasible solution (e.g. after a MIP solve), the reduced costs stay at 0.
    if (_last_result->has_dual_feasible_solution())
    {
        const auto& rc = _last_result->reduced_costs();
        for (int i = 0; i < get_ncols(); ++i)
        {
            auto it = rc.find(_variables[i]);
            if (it != rc.end())
            {
                reduced_costs[i] = it->second;
            }
        }
    }
    return reduced_costs;
}

std::vector<double> SolverMathOpt::get_mip_primal() const
{
    return primal_values("get_mip_primal");
}

std::vector<double> SolverMathOpt::primal_values(const char* caller) const
{
    if (!_last_result.has_value())
    {
        throw GenericSolverException(std::string(caller) + ": no solution available");
    }

    std::vector<double> primals(get_ncols(), 0.0);
    const auto& vals = _last_result->variable_values();
    for (int i = 0; i < get_ncols(); ++i)
    {
        auto it = vals.find(_variables[i]);
        if (it != vals.end())
        {
            primals[i] = it->second;
        }
    }
    return primals;
}

/*************************************************************************************************
------------------------    Methods to set algorithm or logs levels
---------------------------
*************************************************************************************************/

void SolverMathOpt::set_output_log_level(int loglevel)
{
    _log_level = loglevel;
}

void SolverMathOpt::set_threads(int n_threads)
{
    args_.parameters.threads = n_threads;
}

void SolverMathOpt::set_simplex_iter(int iter)
{
    args_.parameters.iteration_limit = iter;
}

/*************************************************************************************************
-----------------------------------    Internal helpers
--------------------------------
*************************************************************************************************/

void SolverMathOpt::rebuild_from_model()
{
    _last_result.reset();
    _initial_basis.reset();
    _variables.clear();
    _constraints.clear();
    _row_types.clear();

    for (const auto& var: _model->SortedVariables())
    {
        _variables.push_back(var);
    }

    // MathOpt stores constraints as lb <= expr <= ub with no row type.
    // SolverAbstract uses row type chars (L/G/E/R) that get_rhs(), chg_rhs(),
    // and get_row_type() depend on, so we derive and cache them from bounds.
    for (const auto& ct: _model->SortedLinearConstraints())
    {
        _constraints.push_back(ct);

        double lb = _model->lower_bound(ct);
        double ub = _model->upper_bound(ct);
        if (lb == ub)
        {
            _row_types.push_back('E');
        }
        else if (lb <= -kInf)
        {
            _row_types.push_back('L');
        }
        else if (ub >= kInf)
        {
            _row_types.push_back('G');
        }
        else
        {
            _row_types.push_back('R');
        }
    }

    _minimize = !_model->is_maximize();

    rebuild_index_maps();
}

void SolverMathOpt::rebuild_index_maps()
{
    _col_name_to_index.clear();
    for (int i = 0; i < static_cast<int>(_variables.size()); ++i)
    {
        std::string n(_model->name(_variables[i]));
        if (!n.empty())
        {
            _col_name_to_index[n] = i;
        }
    }

    _row_name_to_index.clear();
    for (int i = 0; i < static_cast<int>(_constraints.size()); ++i)
    {
        std::string n(_model->name(_constraints[i]));
        if (!n.empty())
        {
            _row_name_to_index[n] = i;
        }
    }
}

void SolverMathOpt::check_range(int first, int last, int size, const char* caller)
{
    // An empty range (last == first - 1) is allowed, e.g. on a problem with no rows.
    if (first < 0 || last >= size || last < first - 1)
    {
        throw std::out_of_range(std::string(caller) + ": range [" + std::to_string(first) + ", "
                                + std::to_string(last) + "] outside [0, " + std::to_string(size - 1)
                                + "]");
    }
}
