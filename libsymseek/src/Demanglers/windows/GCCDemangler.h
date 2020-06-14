#pragma once

#include <symseek/IDemangler.h>

namespace SymSeek
{
    class GCCDemangler: public IDemangler
    {
    public:
        GCCDemangler();

        std::optional<String> demangleName(char const * name) const override;
    };
}
