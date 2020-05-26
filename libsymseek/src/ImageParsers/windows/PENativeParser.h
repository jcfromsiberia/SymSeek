#pragma once

#include <symseek/Definitions.h>

#if !SYMSEEK_OS_WIN()
#   error Unsupported platform
#endif

#include <memory>
#include <type_traits>

#include <symseek/IImageParser.h>

namespace SymSeek
{
    class PENativeParser: public IImageParser
    {
    public:
        ISymbolReader::UPtr reader(String const & imagePath) const override;
    };
}
