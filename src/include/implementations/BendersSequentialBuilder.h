#pragma once

#include <memory>

#include "implementations/BendersSequential.h"
#include "utilities/options/BendersOptionsManager.h"

/*!
 *  \brief Build a BendersSequential ready to launch from a study read by BendersOptionsManager.
 *
 *  Creates OUTPUTROOT and the run's outputs: a logger to the console and
 *  OUTPUTROOT/reportbenders.txt, a JSON writer to OUTPUTROOT/JSON_FILE (starting from the existing
 *  file in resume mode) and a math logger to OUTPUTROOT/benders_solver.log (and the console when
 *  LOG_LEVEL > 0). The manager's warnings are logged.
 */
std::unique_ptr<BendersSequential> BuildBendersSequential(const BendersOptionsManager& manager);
