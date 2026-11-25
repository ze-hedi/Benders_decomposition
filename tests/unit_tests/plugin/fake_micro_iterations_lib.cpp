// Minimal micro-iterations library exporting every hook under its unmangled name, as the
// xpansion Julia library does. Used to test DynamicLibraryLoader.
#include "plugin/MicroIterationsHooks.h"

extern "C"
{
int fake_iteration_start_calls = 0;

void OnBendersStart(std::filesystem::path, std::filesystem::path, bool, void*, int)
{
}

void OnBendersEnd()
{
}

void OnBendersIterationStart()
{
    ++fake_iteration_start_calls;
}

void OnBendersIterationEnd()
{
}

void OnBendersMasterResolutionStart()
{
}

void OnBendersMasterResolutionEnd(std::map<std::string, double>&,
                                  int&,
                                  void*,
                                  std::filesystem::path)
{
}

void OnBendersMicroIterationStart()
{
}

void OnBendersMicroIterationEnd(std::string,
                                std::string,
                                std::vector<double>,
                                std::vector<int>&,
                                std::vector<std::string>&,
                                std::filesystem::path,
                                std::vector<std::string>&,
                                int,
                                int)
{
}

void OnBendersSubResolutionStart()
{
}

void OnBendersSubResolutionEnd()
{
}
}
