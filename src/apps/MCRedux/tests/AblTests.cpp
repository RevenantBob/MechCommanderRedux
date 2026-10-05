#include "stdafx.h"
#include "MCTest.h"
#include "abl/ablenv.h"
#include "abl/ablrtn.h"
#include "abl/ablsymt.h"
#include "fakes/MCMemoryFileSource.h"
#include "main/MCGameContext.h"

// ABL's memory (AblMemory, the original's three ABL heaps): what ABLi_init, compiling and running a module take, and
// that ABLi_close gives all of it back.

namespace
{
    /// <summary>Starts ABL as the scenario does, with the sizes SYSTEM.CFG gives (the heap sizes are ignored).</summary>
    void StartAbl()
    {
        ABLi_init(0x40000, 0x40000, 0x40000, 0x2000, 0x10000, 32, 256, nullptr, 0, 0, 0);
    }

    /// <summary>A module that sums 1..4 through a static array and returns the sum.</summary>
    constexpr const char* SUM_MODULE = "module memtest : integer;\r\n"
                                       "var\r\n"
                                       "    static integer total;\r\n"
                                       "    integer i;\r\n"
                                       "    static integer[4] values;\r\n"
                                       "code\r\n"
                                       "    total = 0;\r\n"
                                       "    for i = 0 to 3 do\r\n"
                                       "        values[i] = i + 1;\r\n"
                                       "    endfor;\r\n"
                                       "    for i = 0 to 3 do\r\n"
                                       "        total = total + values[i];\r\n"
                                       "    endfor;\r\n"
                                       "    return(total);\r\n"
                                       "endmodule.\r\n";
}

TEST_CASE("abl: ABLi_close frees everything ABLi_init and a compile made")
{
    AblMemory.Clear();
    StartAbl();
    // The code buffer, the stack, the registries, and the standard routines' symbols and types.
    const size_t afterInit = AblMemory.Count();
    CHECK(afterInit > 0);
    CHECK(SymTableDisplay[0] != nullptr);
    CHECK(IntegerTypePtr != nullptr);
    CHECK_EQ(IntegerTypePtr->size, 4);

    ABLi_close();
    CHECK_EQ(AblMemory.Count(), 0u);
    CHECK_EQ(ABLi_enabled(), 0);
}

TEST_CASE("abl: a module compiled and run from memory gives its blocks back")
{
    MCTestContextScope scope;
    MCMemoryFileSource& files = scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>());
    files.AddFile("data\\missions\\memtest.abl", SUM_MODULE);

    AblMemory.Clear();
    StartAbl();
    const size_t afterInit = AblMemory.Count();
    char fileName[] = "data\\missions\\memtest.abl";
    int32_t numErrors = -1;
    const int32_t handle = ABLi_preProcess(fileName, &numErrors);
    REQUIRE(handle >= 0);
    CHECK_EQ(numErrors, 0);
    // The module's symbols, types, code segment and registry entry.
    CHECK(AblMemory.Count() > afterInit);

    // Compiling the same file again gives the module already registered.
    char again[] = "data\\missions\\memtest.abl";
    CHECK_EQ(ABLi_preProcess(again), handle);

    auto* module = new ABLModule;
    REQUIRE_EQ(module->init(handle), 0);
    const size_t withInstance = AblMemory.Count();
    module->execute(nullptr);
    CHECK_EQ(module->returnVal, 10);

    // A second run starts from the statics the first left: the same sum.
    module->execute(nullptr);
    CHECK_EQ(module->returnVal, 10);
    CHECK_EQ(AblMemory.Count(), withInstance);

    // The instance frees its static data; the static array's own block stays until ABLi_close, as in the original.
    module->destroy();
    delete module;
    CHECK(AblMemory.Count() < withInstance);

    ABLi_close();
    CHECK_EQ(AblMemory.Count(), 0u);
}
