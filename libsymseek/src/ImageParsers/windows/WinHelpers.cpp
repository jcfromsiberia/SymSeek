#include "WinHelpers.h"

#include <memory>
#include <regex>
#include <type_traits>

#include <tchar.h>

#if defined(DBGHELP_FOUND)
#   include <dbghelp.h>
#endif

#if defined(PSAPI_FOUND)
#   include <psapi.h>
#endif

#if defined(__MINGW32__)
#   include <cxxabi.h>
#endif

#include <Debug.h>
#include <Helpers.h>

template<typename T = typename String::value_type>
using RegEx = std::basic_regex<T>;

template<typename T = typename String::const_iterator>
using RegExMatch = std::match_results<T>;

using SymSeek::String;
using SymSeek::detail::toString;

static String demangleName(LPCCH mangledName)
{
    // Undocumented crap. The only doc I found is https://source.winehq.org/WineAPI/__unDNameEx.html
    using MallocFuncPtr = void * (*)(size_t);
    using FreeFuncPtr   = void (*)(void *);
    using UndNamePtr = char * (*)(
            CHAR * buffer,
            LPCCH mangled,
            int buflen,
            MallocFuncPtr,
            FreeFuncPtr,
            void * reserved,
            unsigned short int flags);

    // Trying to find the advanced undecorate function
    auto findUndName = [](TCHAR const * pattern, TCHAR const * dllName) -> UndNamePtr
    {
        HMODULE vcRuntimeModule{};
#if defined(PSAPI_FOUND)
        // 1) scan all modules loaded;
        // 2) find the required one by name;
        // 3) if not found, try to load the system-wide;
        // 4) get `__unDNameEx` pointer from the module and invoke it.
        DWORD requiredBytes{};
        HANDLE currentProcess = GetCurrentProcess();
        // Cannot GetModuleHandle for the VC runtime library,
        // see http://alax.info/blog/1155
        EnumProcessModules(currentProcess, nullptr, 0, &requiredBytes);
        GUARD(requiredBytes);
        DWORD modulesCount = requiredBytes / sizeof(HMODULE);
        auto handles = std::make_unique<HMODULE[]>(modulesCount);
        if (!EnumProcessModules(currentProcess, handles.get(), requiredBytes, &requiredBytes))
        {
            return {};
        }

        for (DWORD i = 0; i < modulesCount; ++i)
        {
            HMODULE module = handles[i];
            GUARD(module);
            TCHAR buffer[4096] = {0};
            GetModuleFileName(module, buffer, sizeof(buffer) / sizeof(TCHAR));
            _tcslwr_s(buffer, sizeof(buffer) / sizeof(TCHAR));
            String baseName = std::find(std::rbegin(buffer), std::rend(buffer), TEXT('\\')).base();
            if (std::regex_match(baseName, RegEx<TCHAR>(pattern)))
            {
                vcRuntimeModule = module;
                break;
            }
        }
#endif  //defined(PSAPI_FOUND)
        if (!vcRuntimeModule && dllName)
        {
            // If the control flow is here, it means using a non-MSVC toolchain.
            // Trying to load the vcruntime manually
            vcRuntimeModule = LoadLibrary(dllName);
            // No FreeLibrary call as the runtime should live while the static __unDNameEx exists.
            // Don't find it as a resource leak.
        }
        UndNamePtr undName{};
        if (vcRuntimeModule)
        {
            undName = reinterpret_cast<UndNamePtr>(::GetProcAddress(vcRuntimeModule, "__unDNameEx"));
        }
        return undName;
    };

    static UndNamePtr __unDNameEx = [&findUndName]() -> UndNamePtr {
        // msvcrt has the obsolete __unDNameEx which doesn't handle move semantics
        // That's why looking for vcruntime first.
        static TCHAR const * names[][2] =
        {
            {TEXT(R"(^vcruntime\d+d?\..+$)"), TEXT("vcruntime140.dll"  )},
            {TEXT(R"(^msvcrt$)"            ), TEXT("msvcrt.dll"        )},
        };

        for (TCHAR const ** namesRow: names)
        {
            if (auto result = findUndName(namesRow[0], namesRow[1]))
            {
                return result;
            }
        }
        return {};
    }();

    String result = toString(mangledName);

    // Prior to undecorating, check if the name has been mangled with the MSVC mangler
    if(*result.begin() == '?')
    {
        CHAR demandledSymbol[8192] = {0};  // should be enough
        auto const undFlags = UNDNAME_COMPLETE | UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_LEADING_UNDERSCORES;
        if(__unDNameEx)
        {
            __unDNameEx(demandledSymbol, mangledName,
                        sizeof(demandledSymbol) / sizeof(CHAR), ::malloc, ::free, /*reserved=*/nullptr, undFlags);
            result = toString(demandledSymbol);
        }
#if defined(DBGHELP_FOUND)
        else
        {
            // Unfortunately this function doesn't handle move semantics,
            // see e.g https://github.com/lucasg/Dependencies/issues/32
            ::UnDecorateSymbolName(mangledName, demandledSymbol, sizeof(demandledSymbol), undFlags);
            result = toString(demandledSymbol);
        }
#endif  //defined(DBGHELP_FOUND)
    }
#if defined(__MINGW32__)
    else if(result.startsWith("_Z"))  // or the GCC mangler
    {
        int status{};
        char * realName = ::abi::__cxa_demangle(mangledName, /*output_buffer=*/nullptr, /*length*/nullptr, &status);
        if(!status)
        {
            result = toString(realName);
            ::free(realName);
        }
    }
#endif  //defined(__MINGW32__)

    return result;
}

namespace SymSeek::detail
{
    Symbol nameToSymbol(LPCCH mangledName)
    {
        String mangledNameStr = toString(mangledName);//??_R4LIBNativeParser@SymSeek
        String demangledNameStr = demangleName(mangledName);

        Symbol result;
        result.mangledName = mangledNameStr;
        result.demangledName = demangledNameStr;

        if (mangledNameStr == demangledNameStr)
        {
            // C function
            return result;
        }
        String name = demangledNameStr;

        using Rx = RegEx<>;
        using RxMatch = RegExMatch<>;
        
        static Rx const constRx{
            TEXT(R"(^.+\W+\s*const\s*(&|&&)?$)"), std::regex::optimize};

        if (RxMatch match; std::regex_match(name, match, constRx))
        {
            result.modifiers |= Symbol::IsConst;
            result.type = NameType::Method;
        }

        static Rx const accessModifierRx{
            TEXT(R"(^(public|protected|private):.+)"), std::regex::optimize};
        if (RxMatch match; std::regex_match(name, match, accessModifierRx))
        {
            result.type = NameType::Method;
            result.access = Access::Public;
            String const & accessStr = match[1];

            if (accessStr == TEXT("protected"))
            {
                result.access = Access::Protected;
            }
            else if (accessStr == TEXT("private"))
            {
                result.access = Access::Private;
            }

            name.erase(0, accessStr.length() + 2 /*colon and space*/);
        }

        static Rx const modifierRx{
            TEXT(R"(^(virtual|static).+)"), std::regex::optimize};
        if (RxMatch match; std::regex_match(name, match, modifierRx))
        {
            String const & modifier = match[1];
            if (modifier == TEXT("static"))
            {
                result.modifiers |= Symbol::IsStatic;
            }
            else
            {
                result.modifiers |= Symbol::IsVirtual;
            }

            name.erase(0, modifier.length() + 1/*space*/);
        }

        static Rx const signatureRx{
            TEXT( R"(^(.+)\((.*)\)(\s*const\s*)?(&|&&)?$)"), std::regex::optimize};
        if (RxMatch match; !std::regex_match(name, match, signatureRx))
        {
            result.type = NameType::Variable;
            if (name.find(TEXT("const ")) != String::npos)
            {
                result.modifiers |= Symbol::IsConst;
            }
        }

        result.demangledName = name;

        return result;
    }
}
