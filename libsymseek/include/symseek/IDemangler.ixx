module;

#include <symseek/Definitions.h>

export module symseek.interfaces.demangler;

import <memory>;
import <optional>;
import <string>;

export namespace SymSeek
{
    class IDemangler
    {
    public:
        using UPtr = std::unique_ptr<IDemangler>;

        virtual std::optional<std::string> demangleName(char const * name) const = 0;
    };
}
