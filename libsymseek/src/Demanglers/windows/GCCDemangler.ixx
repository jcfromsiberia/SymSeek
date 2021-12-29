module;

#include <Windows.h>

export module symseek:demanglers.gcc;

import <mutex>;

import symseek.interfaces.demangler;
import symseek.internal.helpers;

export namespace SymSeek
{
    class GCCDemangler : public IDemangler
    {
    public:
        GCCDemangler();

        std::optional<std::string> demangleName(char const* name) const override;
    };
}

// Implementation

using namespace SymSeek;

using DemanglePtr =
    char * (*)(
        char const * mangled_name, 
        char * output_buffer, 
        size_t * length, 
        int * status);
using FreePtr = void (*)(void * ptr);

namespace
{
    std::once_flag funcInitFlag;
    DemanglePtr demangle = nullptr;
    FreePtr freeFunc = nullptr;
}

GCCDemangler::GCCDemangler()
{
    auto initDemangleFunction = []()
    {
        static TCHAR const * runtimeLibNames[][2] =
        {
            {TEXT(R"(^libstdc\+\+\-6\.dll$)"), TEXT("libstdc++-6.dll")},
            // TODO LoadLibrary("cygstdc++-6.dll") crashes. Investigate.
            //{TEXT(R"(^cygstdc\+\+\-6\.dll$)"), TEXT("cygstdc++-6.dll")},
        };
        for (TCHAR const ** namesRow: runtimeLibNames)
        {
            if (auto result = detail::findNameInRuntime(
                namesRow[0], namesRow[1], "__cxa_demangle"))
            {
                demangle = reinterpret_cast<DemanglePtr>(result);
                break;
            }
        }

        // TODO turns out MinGW uses func `free` from the obsolete MSVC runtime
        // Find more robust way to explicitly fetch function `free` from the MinGW runtime lib
        static TCHAR const * auxLibNames[][2] = 
        {
            {TEXT(R"(msvcrt\.dll)"), TEXT("msvcrt.dll")}
        };
        for (TCHAR const ** namesRow: auxLibNames)
        {
            if (auto result = detail::findNameInRuntime(
                namesRow[0], namesRow[1], "free"))
            {
                freeFunc = reinterpret_cast<FreePtr>(result);
                break;
            }
        }
    };
    std::call_once(funcInitFlag, initDemangleFunction);
}

std::optional<std::string> GCCDemangler::demangleName(char const * name) const
{
    if (!name || std::memcmp(name, "_Z", 2))
    {
        return std::nullopt;
    }

    if (!demangle || !freeFunc)
    {
        return std::nullopt;
    }

    int status{};
    char * realName = 
        demangle(name, /*output_buffer=*/nullptr, /*length*/nullptr, &status);
    if (status)
    {
        return std::nullopt;
    }

    std::string result = realName;
    freeFunc(realName);
    return std::move(result);
}
