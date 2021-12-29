module;

// Even though the parser doesn't rely on WinAPI, it is using the win-specific demangler ;(
#include <symseek/Definitions.h>

#if !SYMSEEK_OS_WIN()
#   error Unsupported platform
#endif

#include <Windows.h>

#include <Debug.h>

export module symseek:parsers.coff;

import <memory>;

import symseek.definitions;
import symseek.interfaces.parser;

import symseek.internal.interfaces.mappedfile;
import symseek.internal.helpers;

export namespace SymSeek
{
    class COFFNativeParser : public IImageParser
    {
    public:
        ISymbolReader::UPtr reader(String const& imagePath) const override;
    };
}

// Implementation

using namespace SymSeek;
using SymSeek::detail::IMappedFile;

namespace
{
    class COFFNativeSymbolReader : public ISymbolReader
    {
    public:
        COFFNativeSymbolReader(std::unique_ptr<IMappedFile> objectFile)
        : m_objectFile{ std::move(objectFile) }
        {
            uint8_t const * mapped = m_objectFile->map(/*offset=*/0);

            // See https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#coff-file-header-object-and-image
            uint32_t symTableOffset{ *reinterpret_cast<uint32_t const *>(mapped + 8) };
            m_symbolsCount = *reinterpret_cast<uint32_t const *>(mapped + 12);
            m_symTable = mapped + symTableOffset;
        }

        size_t symbolsCount() const override
        {
            return m_symbolsCount;
        }

        SymbolsGen readSymbols() const override
        {
            // See https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#coff-symbol-table
#pragma pack(push, 1)
            struct SymTableEntry
            {
                union
                {
                    char          shortName[8];
                    struct
                    {
                        uint32_t  zeroes;
                        uint32_t  offset;
                    };
                } name;
                uint32_t  value;
                int16_t   sectionNumber;
                uint16_t  type;
                int8_t    storageClass;
                uint8_t   numberOfAuxSymbols;
            };
#pragma pack(pop)
            static_assert(sizeof(SymTableEntry) == 18);

            SymTableEntry const * currentEntry = reinterpret_cast<SymTableEntry const *>(m_symTable);
            SymTableEntry const * end          = currentEntry + m_symbolsCount;

            // See https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#coff-string-table
            LPCCH stringTable = reinterpret_cast<LPCCH>(end);

            for (; currentEntry != end; ++currentEntry)
            {
                // Only this class of symbols matters,
                // see https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#storage-class
                if (currentEntry->storageClass != IMAGE_SYM_CLASS_EXTERNAL)
                {
                    continue;
                }
                LPCCH rawSymbolName{};
                if(!currentEntry->name.zeroes)
                {
                    // The symbol name is longer than 8 bytes and put into the string table.
                    rawSymbolName = stringTable + currentEntry->name.offset;
                }
                else
                {
                    rawSymbolName = currentEntry->name.shortName;
                }
                // See https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#section-number-values
                bool const undefined = currentEntry->sectionNumber == IMAGE_SYM_UNDEFINED;
                if (undefined)
                {
                    // Check if the name starts with __imp_ and remove this prefix
                    // Ugly, but optimal, no need to call strlen nor create a std::string
                    if(!std::memcmp(rawSymbolName, "__imp_", 6))
                    {
                        rawSymbolName += 6;
                    }
                }

                co_yield RawSymbol{.name = rawSymbolName, .implements = !undefined};
            }
        }

    private:
        std::unique_ptr<IMappedFile> m_objectFile;
        uint32_t m_symbolsCount{};
        uint8_t const * m_symTable{};
    };
}

ISymbolReader::UPtr COFFNativeParser::reader(String const & imagePath) const
{
    auto objectFile = detail::createMappedFile(imagePath);
    GUARD(objectFile->isOpen());

    // See https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#coff-file-header-object-and-image

    uint32_t signature{};
    objectFile->read(reinterpret_cast<char*>(&signature), sizeof(uint32_t));
    if (signature == 0x0000FFFF)
    {
        // This is a special case when the object file has been compiled with
        // Whole Program Optimization (/GL) flag, which is the fuel for Link-Time Code Generation
        // It doesn't match the COFF specification :(
        // Maybe this will be reverse-engineered later.
        return {};
    }

    auto machine = signature & 0x0000FFFF;
    if(machine != IMAGE_FILE_MACHINE_AMD64 && machine != IMAGE_FILE_MACHINE_I386)
    {
        return {};
    }

    return std::make_unique<COFFNativeSymbolReader>(std::move(objectFile));
}
