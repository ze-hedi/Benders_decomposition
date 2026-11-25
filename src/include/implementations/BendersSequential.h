#pragma once

#include <memory>
#include <string>

#include "benders_managers/BendersCutsManagerSequential.h"
#include "benders_managers/BendersSubProblemsManagerSequential.hxx"
#include "implementations/BendersBase.h"

/*!
 * \class BendersSequential
 * \brief Runs the Benders algorithm in a single process
 */
class BendersSequential: public BendersBase
{
public:
    BendersSequential(const BendersBaseOptions& options,
                      Logger logger,
                      std::shared_ptr<Output::OutputWriter> writer,
                      std::shared_ptr<MathLoggerDriver> mathLoggerDriver);
    ~BendersSequential() override = default;

    void launch() override;
    virtual void BuildCut();
    void InitializeProblems() override;

    [[nodiscard]] std::string BendersName() const override
    {
        return "Sequential";
    }

protected:
    void free() override;
    void Run() override;

    std::unique_ptr<BendersSubProblemsManagerSequential> subproblems_manager_;

private:
    std::unique_ptr<BendersCutsManagerSequential> cuts_manager_;
};
