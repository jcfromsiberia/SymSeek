#pragma once

#include <memory>
#include <string>
#include <type_traits>

#include <symseek/Definitions.h>

#include <MappedFile/IMappedFile.h>

namespace SymSeek::detail
{
    [[nodiscard]] std::unique_ptr<IMappedFile> createMappedFile(String const & filePath);

    template<typename T>
    [[nodiscard]] String toString(T value) requires std::is_fundamental_v<T>
    {
        if constexpr(std::is_same_v<String::value_type, wchar_t>)
        {
            return std::to_wstring(value);
        }
        else
        {
            return std::to_string(value);
        }
    }
}
