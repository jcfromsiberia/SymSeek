#pragma once

// Even though the parser doesn't rely on WinAPI, it is using the win-specific demangler ;(
#include <symseek/Definitions.h>

#if !SYMSEEK_OS_WIN()
#   error Unsupported platform
#endif

#include <memory>

#include <symseek/IImageParser.h>

#include "src/MappedFile/IMappedFile.h"

namespace SymSeek
{
    class LIBNativeParser : public IImageParser
    {
    public:
        ISymbolReader::UPtr reader(String const & imagePath) const override;
    };

    class LIBNativeSymbolReader : public ISymbolReader
    {
    public:
        LIBNativeSymbolReader(std::unique_ptr<detail::IMappedFile> archiveFile);

        size_t symbolsCount() const override;

        SymbolsGen readSymbols() const override;

    private:
        void readSymbolsCount();

    private:
        std::unique_ptr<detail::IMappedFile> m_archiveFile;
        uint32_t m_symbolsCount{};
    };
}
