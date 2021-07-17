#include "MSVCDemangler.h"

#include <mutex>
#include <Windows.h>

#if defined(DBGHELP_FOUND)
#   include <dbghelp.h>
#endif

#if defined(PSAPI_FOUND)
#   include <psapi.h>
#endif

#include <Helpers.h>

using namespace SymSeek;

// Undocumented crap. The only doc I found was https://source.winehq.org/WineAPI/__unDNameEx.html
using UndNamePtr = 
    char * (*)(
        CHAR * buffer,
        LPCCH mangled,
        int buflen,
        void * (*)(size_t),
        void (*)(void *),
        void * reserved,
        unsigned short flags);

using UnDecorateSymbolNamePtr = 
    DWORD (*)(
        PCSTR name,
        PSTR outputString,
        DWORD maxStringLength,
        DWORD flags);

namespace
{
    std::once_flag funcsInitFlag;
    UndNamePtr              undName        = nullptr;
    UnDecorateSymbolNamePtr undecorateName = nullptr;
}

MSVCDemangler::MSVCDemangler()
{
    auto initUndFunctions = []()
    {
        // Trying to find the advanced undecorate function
        static TCHAR const * names[][2] =
        {
            {TEXT(R"(^vcruntime\d+d?\..+$)"), TEXT("vcruntime140.dll"  )},
            {TEXT(R"(^msvcrt$)"            ), TEXT("msvcrt.dll"        )},
        };
        for (TCHAR const ** namesRow: names)
        {
            if (auto result = detail::findNameInRuntime(namesRow[0], namesRow[1], "__unDNameEx"))
            {
                undName = reinterpret_cast<UndNamePtr>(result);
                break;
            }
        }
#if defined(DBGHELP_FOUND)
        undecorateName = ::UnDecorateSymbolName;
#else
        undecorateName = reinterpret_cast<UnDecorateSymbolNamePtr>(
            detail::findNameInRuntime(
                TEXT(R"(^dbghelp\.dll$)"), TEXT("dbghelp.dll"), "UnDecorateSymbolName"));
#endif
    };

    std::call_once(funcsInitFlag, initUndFunctions);
}

std::optional<std::string> MSVCDemangler::demangleName(char const * name) const
{
    if (!name || *name != '?')
    {
        return std::nullopt;
    }

    CHAR demandledSymbol[8192] = {0};  // should be enough

    constexpr unsigned short undFlags =
#if defined(DBGHELP_FOUND)
        UNDNAME_COMPLETE | UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_LEADING_UNDERSCORES;
    static_assert(undFlags == 3);
#else
        3;
#endif 

    if (undName)
    {
         undName(demandledSymbol, name, sizeof(demandledSymbol) / sizeof(CHAR), 
             ::malloc, ::free, /*reserved=*/nullptr, undFlags);
         return demandledSymbol;
    }
    else if (undecorateName)
    {
        undecorateName(name, demandledSymbol, sizeof(demandledSymbol), undFlags);
        return demandledSymbol;
    }

    return std::nullopt;
}
