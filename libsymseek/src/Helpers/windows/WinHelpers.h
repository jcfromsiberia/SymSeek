#pragma once

#include <type_traits>

#include <symseek/Definitions.h>

#if !SYMSEEK_OS_WIN()
#   error Unsupported platform
#endif

#include <symseek/symseek.h>

#include <Windows.h>

namespace SymSeek::detail
{
    template<typename T = String>
    T toString(LPCCH str)
    {
        if constexpr(std::is_same_v<typename T::value_type, wchar_t>)
        {
            size_t const length = std::strlen(str);
            T result(length, 0);

            ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str, 
                /*cbMultiByte=*/-1, result.data(), static_cast<int>(length));
            return result;
        }
        else
        {
            return str;
        }
    }

    LPVOID findNameInRuntime(LPCTSTR pattern, LPCTSTR dllName, LPCSTR name);
}
