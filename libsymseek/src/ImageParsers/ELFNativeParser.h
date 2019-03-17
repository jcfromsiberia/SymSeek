#pragma once

#include <elf.h>

#include "IImageParser.h"

namespace SymSeek
{
    namespace detail
    {
        using ElfSymHeaderArray = std::unique_ptr<Elf64_Shdr[]>;
    }

    class ELFNativeParser : public IImageParser
    {
    public:
        ISymbolReader::UPtr reader(QString imagePath) const override;
    };

    class ELFNativeSymbolReader : public ISymbolReader
    {
    public:
        ELFNativeSymbolReader(detail::ElfSymHeaderArray && elfSymbols, size_t count);

        size_t symbolsCount() const override;

        void readInto(SymbolsInserter outputIter, SymbolHandler handler = {}) const override;

    private:
        detail::ElfSymHeaderArray m_elfSymbols;
        size_t m_count;
    };
}