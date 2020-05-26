#pragma once

// Even though the parser doesn't rely on WinAPI, it is using the win-specific demangler ;(
#include <symseek/Definitions.h>

#if !SYMSEEK_OS_WIN()
#   error Unsupported platform
#endif

#include <memory>

#include <symseek/IImageParser.h>

namespace SymSeek
{
    class COFFNativeParser: public IImageParser
    {
    public:
        ISymbolReader::UPtr reader(String const & imagePath) const override;
    };
}
