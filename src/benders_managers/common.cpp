#include "benders_managers/common.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <json/reader.h>

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

Json::Value get_json_file_content(const std::filesystem::path& json_file)
{
    std::ifstream input_file_l(json_file, std::ifstream::binary);

    Json::CharReaderBuilder builder_l;
    Json::Value ret;
    // json file content
    std::string errs;
    if (!parseFromStream(builder_l, input_file_l, &ret, &errs))
    {
        std::cerr << std::endl << "Invalid Json file: " << json_file.string() << std::endl;
        std::cerr << errs << std::endl;
    }
    return ret;
}
