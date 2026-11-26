#pragma once

/**
 * @brief Single include for MPI code: Boost.MPI plus the serialization headers needed to send the
 * Benders data types (Point, SubProblemDataMap, SubProblemNamesInCut, ...) between processes.
 */

#include <boost/mpi.hpp>
#include <boost/serialization/deque.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>

namespace mpi = boost::mpi;
