#include "WinHelpers.h"

#include <memory>
#include <regex>

#include <tchar.h>

#if defined(PSAPI_FOUND)
#   include <psapi.h>
#endif

#include <Debug.h>
#include <Helpers.h>

template<typename T = typename String::value_type>
using RegEx = std::basic_regex<T>;

template<typename T = typename String::const_iterator>
using RegExMatch = std::match_results<T>;

using SymSeek::String;
using SymSeek::detail::toString;

namespace SymSeek::detail
{
    LPVOID findNameInRuntime(LPCTSTR pattern, LPCTSTR dllName, LPCSTR name)
    {
        HMODULE runtimeModule{};
#if defined(PSAPI_FOUND)
        // 1) scan all modules loaded;
        // 2) find the required one by name;
        // 3) if not found, try to load the system-wide;
        // 4) get `__unDNameEx` pointer from the module and invoke it.
        DWORD requiredBytes{};
        HANDLE currentProcess = GetCurrentProcess();
        // Cannot GetModuleHandle for the VC runtime library,
        // see http://alax.info/blog/1155
        ::EnumProcessModules(currentProcess, nullptr, 0, &requiredBytes);
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
            ::GetModuleFileName(module, buffer, sizeof(buffer) / sizeof(TCHAR));
            ::_tcslwr_s(buffer, sizeof(buffer) / sizeof(TCHAR));
            String baseName = std::find(std::rbegin(buffer), std::rend(buffer), TEXT('\\')).base();
            if (std::regex_match(baseName, RegEx<TCHAR>(pattern)))
            {
                runtimeModule = module;
                break;
            }
        }
#endif  //defined(PSAPI_FOUND)
        if (!runtimeModule && dllName)
        {
            // Trying to load the runtime manually
            runtimeModule = ::LoadLibrary(dllName);
            // No FreeLibrary call as the runtime should live while the function exists.
            // Don't find it as a resource leak.
        }

        LPVOID result = nullptr;
        if (runtimeModule)
        {
            result = ::GetProcAddress(runtimeModule, name);
        }
        return result;
    }
}
