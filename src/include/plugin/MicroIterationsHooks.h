#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

/*
  Signatures of the functions exported by the micro-iterations library (written in Julia in
  xpansion). They match xpansion's so that existing libraries keep working: the
  mpi::communicator* argument is kept as an opaque pointer, always nullptr here.
*/
using on_Benders_start_Func = void (*)(std::filesystem::path,
                                       std::filesystem::path,
                                       bool,
                                       void* world,
                                       int);
using on_Benders_end_Func = void (*)();
using on_Benders_iteration_start = void (*)();
using on_Benders_iteration_end = void (*)();
using on_Benders_master_resolution_start = void (*)();
using on_Benders_master_resolution_end = void (*)(std::map<std::string, double>&,
                                                  int&,
                                                  void* world,
                                                  std::filesystem::path input_root);
using on_Benders_micro_iteration_start = void (*)();
using on_Benders_micro_iteration_end = void (*)(std::string sub_name,
                                                std::string solving_time,
                                                std::vector<double> sub_solution,
                                                std::vector<int>& variables_indices_vector,
                                                std::vector<std::string>& variables_names_vector,
                                                std::filesystem::path input_root,
                                                std::vector<std::string>& contraints_to_add_vec,
                                                int,
                                                int);
using on_Benders_sub_resolution_start = void (*)();
using on_Benders_sub_resolution_end = void (*)();

/*!
 * \struct MicroIterationsHooks
 * \brief The functions of a micro-iterations library, all required
 */
struct MicroIterationsHooks
{
    on_Benders_start_Func OnBendersStart = nullptr;
    on_Benders_end_Func OnBendersEnd = nullptr;
    on_Benders_iteration_start OnBendersIterationStart = nullptr;
    on_Benders_iteration_end OnBendersIterationEnd = nullptr;
    on_Benders_master_resolution_start OnBendersMasterResolutionStart = nullptr;
    on_Benders_master_resolution_end OnBendersMasterResolutionEnd = nullptr;
    on_Benders_micro_iteration_start OnBendersMicroIterationStart = nullptr;
    on_Benders_micro_iteration_end OnBendersMicroIterationEnd = nullptr;
    on_Benders_sub_resolution_start OnBendersSubResolutionStart = nullptr;
    on_Benders_sub_resolution_end OnBendersSubResolutionEnd = nullptr;
};
