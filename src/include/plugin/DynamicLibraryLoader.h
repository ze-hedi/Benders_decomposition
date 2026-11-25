#pragma once

#include <filesystem>
#include <string>

#include "plugin/MicroIterationsHooks.h"

/*!
 * \class DynamicLibraryLoader
 * \brief Owns a dlopen handle, closed on destruction (Linux only)
 */
class DynamicLibraryLoader
{
public:
    /**
     * @throws std::runtime_error if the library cannot be opened
     */
    explicit DynamicLibraryLoader(const std::filesystem::path& library_path);
    ~DynamicLibraryLoader();
    DynamicLibraryLoader(const DynamicLibraryLoader&) = delete;
    DynamicLibraryLoader& operator=(const DynamicLibraryLoader&) = delete;
    DynamicLibraryLoader(DynamicLibraryLoader&&) = delete;
    DynamicLibraryLoader& operator=(DynamicLibraryLoader&&) = delete;

    /**
     * @throws std::runtime_error if the library has no such symbol
     */
    [[nodiscard]] void* Symbol(const std::string& name) const;

    /**
     * @brief Loads every micro-iterations hook
     *
     * @throws std::runtime_error if one is missing
     */
    [[nodiscard]] MicroIterationsHooks LoadMicroIterationsHooks() const;

private:
    std::filesystem::path library_path_;
    void* handle_ = nullptr;
};
