#include "LIBNativeParser.h"

#include <algorithm>

#include <Debug.h>

#include <Helpers.h>

#include "WinHelpers.h"

using namespace SymSeek;

ISymbolReader::UPtr LIBNativeParser::reader(String const & imagePath) const
{
    auto archiveFile = detail::createMappedFile(imagePath);
    GUARD(!!archiveFile);
    // Avoid reading the entire file into a QByteArray as it can be extremely huge.

    // See https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#archive-library-file-format
    char signature[9] = {0};
    archiveFile->read(&signature, sizeof(signature) /*Excluding null terminator*/- 1);

    if (!std::strcmp(signature, "!<arch>\n"))
    {
        return std::make_unique<LIBNativeSymbolReader>(std::move(archiveFile));
    }

    return {};
}

LIBNativeSymbolReader::LIBNativeSymbolReader(std::unique_ptr<detail::IMappedFile> archiveFile)
: m_archiveFile{ std::move(archiveFile) }
{
    readSymbolsCount();
}

size_t LIBNativeSymbolReader::symbolsCount() const
{
    return m_symbolsCount;
}

LIBNativeSymbolReader::SymbolsGen LIBNativeSymbolReader::readSymbols() const
{
    if (!m_symbolsCount)
    {
        co_return;
    }

    size_t const tableBeginOffset = 
        /*Signature=*/8 + /*First_header=*/60 + /*Number_of_symbols=*/4 +
        /*Offsets_array=*/4 * m_symbolsCount;

    // Now we can map the symbol table onto the memory
    uint8_t const * mapped = m_archiveFile->map(tableBeginOffset);
    LPCCH symTable = reinterpret_cast<LPCCH>(mapped);
    for (uint32_t i = 0;
        i < m_symbolsCount;
        ++i, symTable += strlen(symTable) + /*terminator \0*/1)
    {
        Symbol symbol = detail::nameToSymbol(symTable);

        co_yield std::move(symbol);
    }
    m_archiveFile->unmap();
}

void LIBNativeSymbolReader::readSymbolsCount()
{
    GUARD(!!m_archiveFile);
    GUARD(m_archiveFile->isOpen());
    // Skip the 1st mem header https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#archive-member-headers
    m_archiveFile->seek(/*Signature=*/8 + /*First_header=*/60);

    // Number of symbols https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#first-linker-member
    static_assert(sizeof(m_symbolsCount) == 4);
    m_archiveFile->read(&m_symbolsCount, 4);

    // The number is in Big Endian.
    // TODO move to helper functions. It makes this TU more windows-specific
    m_symbolsCount = ::_byteswap_ulong(m_symbolsCount);
}
