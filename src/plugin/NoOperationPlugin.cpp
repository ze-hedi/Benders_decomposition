#include "plugin/NoOperationPlugin.h"

void NoOperationPlugin::OnBendersStart(const SubproblemsMapPtr&,
                                       const BendersBaseOptions&,
                                       const std::filesystem::path&)
{
}

void NoOperationPlugin::OnBendersEnd()
{
}

void NoOperationPlugin::OnBendersIterationStart()
{
}

void NoOperationPlugin::OnBendersIterationEnd()
{
}

void NoOperationPlugin::OnBendersSubResolutionStart(const std::shared_ptr<SubproblemWorker>&,
                                                    const std::string&)
{
}

void NoOperationPlugin::OnBendersSubResolutionEnd()
{
}

void NoOperationPlugin::OnBendersMasterResolutionStart()
{
}

void NoOperationPlugin::OnBendersMasterResolutionEnd(std::map<std::string, double>&, int&)
{
}

void NoOperationPlugin::OnBendersMicroIterationStart()
{
}

void NoOperationPlugin::OnBendersMicroIterationEnd(const std::string&,
                                                   bool& added_rows,
                                                   const std::string&,
                                                   int,
                                                   int)
{
    added_rows = false;
}

bool NoOperationPlugin::ShouldRestoreSubproblemBasis() const
{
    return true;
}
