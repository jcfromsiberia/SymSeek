module;

#include <symseek/Definitions.h>

#if SYMSEEK_OS_WIN()
#include <Windows.h>
#endif

export module symseek.internal.helpers;

import <memory>;
import <string>;
import <type_traits>;

import symseek.definitions;
import symseek.internal.interfaces.mappedfile;

#if SYMSEEK_OS_WIN()
export import symseek.internal.helpers.win;

import symseek.internal.mappedfile.win;
#endif

export namespace SymSeek::detail
{
    [[nodiscard]] std::unique_ptr<IMappedFile> createMappedFile(String const & filePath)
    {
        MappedFile file{};
        file.open(filePath);

        if (!file.isOpen())
        {
            return {};
        }

        return std::make_unique<MappedFile>(std::move(file));
    }

    template <class T>
    concept Fundamental = std::is_fundamental_v<T>;

    template<typename Return = String>
    [[nodiscard]] Return toString(Fundamental auto value)
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
