module;

#include <symseek/Definitions.h>

export module symseek.definitions;

import <cstdint>;
import <string>;

export namespace SymSeek
{
#if SYMSEEK_OS_WIN()
#    if defined(UNICODE) || defined(_UNICODE)
    using String = std::wstring;
#    else
    using String = std::string;
#    endif
#else
    using String = std::string;
#endif

    enum class Mangler: uint8_t
    {
        MSVC = 0,
        GCC
    };
}
