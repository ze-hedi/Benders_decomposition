#include "plugin/DynamicLibraryLoader.h"

#include <dlfcn.h>
#include <stdexcept>

DynamicLibraryLoader::DynamicLibraryLoader(const std::filesystem::path& library_path):
    library_path_(library_path),
    handle_(dlopen(library_path.c_str(), RTLD_NOW))
{
    if (handle_ == nullptr)
    {
        const char* error = dlerror();
        throw std::runtime_error("failed to open the plugin given on path " + library_path.string()
                                 + ": " + (error ? error : "unknown"));
    }
}

DynamicLibraryLoader::~DynamicLibraryLoader()
{
    dlclose(handle_);
}

void* DynamicLibraryLoader::Symbol(const std::string& name) const
{
    void* symbol = dlsym(handle_, name.c_str());
    if (symbol == nullptr)
    {
        throw std::runtime_error("can't find " + name + " in the plugin " + library_path_.string());
    }
    return symbol;
}

MicroIterationsHooks DynamicLibraryLoader::LoadMicroIterationsHooks() const
{
    MicroIterationsHooks hooks;
    hooks.OnBendersStart = reinterpret_cast<on_Benders_start_Func>(Symbol("OnBendersStart"));
    hooks.OnBendersEnd = reinterpret_cast<on_Benders_end_Func>(Symbol("OnBendersEnd"));
    hooks.OnBendersIterationStart = reinterpret_cast<on_Benders_iteration_start>(
      Symbol("OnBendersIterationStart"));
    hooks.OnBendersIterationEnd = reinterpret_cast<on_Benders_iteration_end>(
      Symbol("OnBendersIterationEnd"));
    hooks.OnBendersMasterResolutionStart = reinterpret_cast<on_Benders_master_resolution_start>(
      Symbol("OnBendersMasterResolutionStart"));
    hooks.OnBendersMasterResolutionEnd = reinterpret_cast<on_Benders_master_resolution_end>(
      Symbol("OnBendersMasterResolutionEnd"));
    hooks.OnBendersMicroIterationStart = reinterpret_cast<on_Benders_micro_iteration_start>(
      Symbol("OnBendersMicroIterationStart"));
    hooks.OnBendersMicroIterationEnd = reinterpret_cast<on_Benders_micro_iteration_end>(
      Symbol("OnBendersMicroIterationEnd"));
    hooks.OnBendersSubResolutionStart = reinterpret_cast<on_Benders_sub_resolution_start>(
      Symbol("OnBendersSubResolutionStart"));
    hooks.OnBendersSubResolutionEnd = reinterpret_cast<on_Benders_sub_resolution_end>(
      Symbol("OnBendersSubResolutionEnd"));
    return hooks;
}
