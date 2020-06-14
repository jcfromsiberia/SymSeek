#pragma once

#include <symseek/IDemangler.h>

namespace SymSeek
{
    class MSVCDemangler: public IDemangler
    {
    public:
        MSVCDemangler();

        std::optional<std::string> demangleName(char const * name) const override;
    };
}
