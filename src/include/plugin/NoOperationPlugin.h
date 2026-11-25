#pragma once

#include "plugin/BendersPlugin.h"

/*!
 * \class NoOperationPlugin
 * \brief Plugin whose callbacks do nothing, used when no plugin is requested
 */
class NoOperationPlugin final: public BendersPlugin
{
public:
    void OnBendersStart(const SubproblemsMapPtr& subproblem_map,
                        const BendersBaseOptions& options,
                        const std::filesystem::path& solver_log_file) override;
    void OnBendersEnd() override;
    void OnBendersIterationStart() override;
    void OnBendersIterationEnd() override;
    void OnBendersSubResolutionStart(const std::shared_ptr<SubproblemWorker>& sub_worker,
                                     const std::string& sub_name) override;
    void OnBendersSubResolutionEnd() override;
    void OnBendersMasterResolutionStart() override;
    void OnBendersMasterResolutionEnd(std::map<std::string, double>& master_out,
                                      int& num_iter) override;
    void OnBendersMicroIterationStart() override;
    void OnBendersMicroIterationEnd(const std::string& sub_name,
                                    bool& added_rows,
                                    const std::string& solve_time,
                                    int num_master_iter,
                                    int num_micro_iter) override;
    [[nodiscard]] bool ShouldRestoreSubproblemBasis() const override;
};
