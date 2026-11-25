// ===========================================================================
//
//  Unit tests for DynamicLibraryLoader and NoOperationPlugin.
//  FAKE_LIB / FAKE_INCOMPLETE_LIB are shared libraries built by CMake from
//  fake_micro_iterations_lib.cpp and fake_incomplete_lib.cpp.
//
// ===========================================================================

#include <stdexcept>

#include "gtest/gtest.h"
#include "plugin/DynamicLibraryLoader.h"
#include "plugin/NoOperationPlugin.h"

TEST(DynamicLibraryLoaderTest, MissingLibraryThrows)
{
    EXPECT_THROW(DynamicLibraryLoader("/nonexistent/libmissing.so"), std::runtime_error);
}

TEST(DynamicLibraryLoaderTest, LoadsEveryHook)
{
    DynamicLibraryLoader loader(FAKE_LIB);

    auto hooks = loader.LoadMicroIterationsHooks();

    EXPECT_NE(hooks.OnBendersStart, nullptr);
    EXPECT_NE(hooks.OnBendersEnd, nullptr);
    EXPECT_NE(hooks.OnBendersIterationStart, nullptr);
    EXPECT_NE(hooks.OnBendersIterationEnd, nullptr);
    EXPECT_NE(hooks.OnBendersMasterResolutionStart, nullptr);
    EXPECT_NE(hooks.OnBendersMasterResolutionEnd, nullptr);
    EXPECT_NE(hooks.OnBendersMicroIterationStart, nullptr);
    EXPECT_NE(hooks.OnBendersMicroIterationEnd, nullptr);
    EXPECT_NE(hooks.OnBendersSubResolutionStart, nullptr);
    EXPECT_NE(hooks.OnBendersSubResolutionEnd, nullptr);
}

TEST(DynamicLibraryLoaderTest, LoadedHookCallsLibraryFunction)
{
    DynamicLibraryLoader loader(FAKE_LIB);
    auto hooks = loader.LoadMicroIterationsHooks();
    auto* calls = static_cast<int*>(loader.Symbol("fake_iteration_start_calls"));
    const int before = *calls;

    hooks.OnBendersIterationStart();

    EXPECT_EQ(*calls, before + 1);
}

TEST(DynamicLibraryLoaderTest, MissingSymbolThrows)
{
    DynamicLibraryLoader loader(FAKE_LIB);

    EXPECT_THROW((void)loader.Symbol("NotAHook"), std::runtime_error);
}

TEST(DynamicLibraryLoaderTest, MissingHookThrows)
{
    DynamicLibraryLoader loader(FAKE_INCOMPLETE_LIB);

    EXPECT_THROW((void)loader.LoadMicroIterationsHooks(), std::runtime_error);
}

TEST(NoOperationPluginTest, ReportsNoAddedRowsAndRestoresBasis)
{
    NoOperationPlugin plugin;
    bool added_rows = true;

    plugin.OnBendersMicroIterationEnd("sub", added_rows, "0", 1, 1);

    EXPECT_FALSE(added_rows);
    EXPECT_TRUE(plugin.ShouldRestoreSubproblemBasis());
}
