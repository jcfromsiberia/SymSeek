#pragma once

#include <memory>
#include <string>
#include <type_traits>

#include <symseek/Definitions.h>

#include <MappedFile/IMappedFile.h>

#if SYMSEEK_OS_WIN()
#   include "Helpers/windows/WinHelpers.h"
#endif

namespace SymSeek::detail
{
    [[nodiscard]] std::unique_ptr<IMappedFile> createMappedFile(String const & filePath);

    template<typename T, typename Return = String>
    [[nodiscard]] Return toString(T value) requires std::is_fundamental_v<T>
    {
        if constexpr(std::is_same_v<typename Return::value_type, wchar_t>)
        {
            return std::to_wstring(value);
        }
        else
        {
            return std::to_string(value);
        }
    }
}
