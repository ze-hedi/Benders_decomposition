#include "benders_managers/common.h"

#include <cmath>

double norm_point(const Point& x0, const Point& x1)
{
    double result(0);
    for (auto& kvp: x0)
    {
        result += (x0.find(kvp.first)->second - x1.find(kvp.first)->second)
                  * (x0.find(kvp.first)->second - x1.find(kvp.first)->second);
    }
    result = std::sqrt(result);
    return result;
}
