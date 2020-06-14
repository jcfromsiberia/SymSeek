#pragma once

#include <memory>
#include <optional>

#include "Definitions.h"

namespace SymSeek
{
    class IDemangler
    {
    public:
        using UPtr = std::unique_ptr<IDemangler>;

        virtual std::optional<std::string> demangleName(char const * name) const = 0;
    };
}
