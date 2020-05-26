#include "PENativeParser.h"

#include <algorithm>
#include <type_traits>

#include <Windows.h>

#include <Debug.h>
#include <Helpers.h>

#include "WinHelpers.h"

using namespace SymSeek;

template<WORD Machine>
struct PETypes;

template<>
struct PETypes<IMAGE_FILE_MACHINE_I386>
{
    using BytePtr             = LPCBYTE;
    using DOSHeaderPtr        = IMAGE_DOS_HEADER const *;
    using ExportDirectoryPtr  = IMAGE_EXPORT_DIRECTORY const *;
    using ImportByNamePtr     = IMAGE_IMPORT_BY_NAME const *;
    using ImportDescriptorPtr = IMAGE_IMPORT_DESCRIPTOR const *;
    using NTHeaders           = IMAGE_NT_HEADERS32;
    using NTHeadersPtr        = IMAGE_NT_HEADERS32 const *;
    using OptionalHeaderPtr   = IMAGE_OPTIONAL_HEADER32 const *;
    using SectionHeaderPtr    = IMAGE_SECTION_HEADER const *;
    using ThunkDataPtr        = IMAGE_THUNK_DATA32 const *;

    static constexpr auto ImageOrdinalFlag = IMAGE_ORDINAL_FLAG32;
};

template<>
struct PETypes<IMAGE_FILE_MACHINE_AMD64>
{
    using BytePtr             = LPCBYTE;
    using DOSHeaderPtr        = IMAGE_DOS_HEADER const *;
    using ExportDirectoryPtr  = IMAGE_EXPORT_DIRECTORY const *;
    using ImportByNamePtr     = IMAGE_IMPORT_BY_NAME const *;
    using ImportDescriptorPtr = IMAGE_IMPORT_DESCRIPTOR const *;
    using NTHeaders           = IMAGE_NT_HEADERS64;
    using NTHeadersPtr        = IMAGE_NT_HEADERS64 const *;
    using OptionalHeaderPtr   = IMAGE_OPTIONAL_HEADER64 const *;
    using SectionHeaderPtr    = IMAGE_SECTION_HEADER const *;
    using ThunkDataPtr        = IMAGE_THUNK_DATA64 const *;

    static constexpr auto ImageOrdinalFlag = IMAGE_ORDINAL_FLAG64;
};

using FileUPtr = std::unique_ptr<detail::IMappedFile>;

namespace SymSeek::detail
{
    template<WORD Machine>
    class PENativeSymbolReader: public ISymbolReader
    {
        // Unfortunately cannot inherit from PETypes<Machine> to pull these types into the context :(
        // MinGW doesn't allow this, Visual Studio does.
        using BytePtr             = typename PETypes<Machine>::BytePtr            ;
        using DOSHeaderPtr        = typename PETypes<Machine>::DOSHeaderPtr       ;
        using ExportDirectoryPtr  = typename PETypes<Machine>::ExportDirectoryPtr ;
        using ImportByNamePtr     = typename PETypes<Machine>::ImportByNamePtr    ;
        using ImportDescriptorPtr = typename PETypes<Machine>::ImportDescriptorPtr;
        using NTHeadersPtr        = typename PETypes<Machine>::NTHeadersPtr       ;
        using OptionalHeaderPtr   = typename PETypes<Machine>::OptionalHeaderPtr  ;
        using SectionHeaderPtr    = typename PETypes<Machine>::SectionHeaderPtr   ;
        using ThunkDataPtr        = typename PETypes<Machine>::ThunkDataPtr       ;

        static constexpr auto ImageOrdinalFlag = PETypes<Machine>::ImageOrdinalFlag;

        template<typename T>
        T map(ULONGLONG virtualAddress) const
        {
            DWORD section{};
            for (; section < m_sectionsCount; ++section)
            {
                DWORD sectionBeginRVA = GUARD(m_sectionHeader)[section].VirtualAddress;
                DWORD sectionEndRVA = sectionBeginRVA + m_sectionHeader[section].Misc.VirtualSize;
                if (sectionBeginRVA <= virtualAddress && virtualAddress <= sectionEndRVA)
                {
                    break;
                }
            }
            
            ULONGLONG offset = GUARD(m_sectionHeader)[section].PointerToRawData +
                virtualAddress - m_sectionHeader[section].VirtualAddress;
            return reinterpret_cast<T>(GUARD(m_moduleBytes) + offset);
        }
    public:
        PENativeSymbolReader(FileUPtr moduleFile, BytePtr moduleBytes)
        : m_moduleFile { std::move(moduleFile) }
        , m_moduleBytes{ moduleBytes           }
        {
            m_dosHeader = reinterpret_cast<DOSHeaderPtr>(m_moduleBytes);
            m_ntHeader = reinterpret_cast<NTHeadersPtr>(m_moduleBytes + m_dosHeader->e_lfanew);

            m_sectionsCount = m_ntHeader->FileHeader.NumberOfSections;
            m_sectionHeader = reinterpret_cast<SectionHeaderPtr>(
                    m_moduleBytes + m_dosHeader->e_lfanew + sizeof(std::remove_pointer_t<NTHeadersPtr>));

            if (DWORD exportAddressOffset = m_ntHeader->
                    OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress; exportAddressOffset)
            {
                m_exportDirectory = map<ExportDirectoryPtr>(exportAddressOffset);
            }

            if (DWORD importAddressOffset = m_ntHeader->
                    OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress; importAddressOffset)
            {
                m_importDescriptor = map<ImportDescriptorPtr>(importAddressOffset);
            }
        }

        size_t symbolsCount() const override
        {
            size_t result{};
            if (m_exportDirectory)
            {
                result += m_exportDirectory->NumberOfNames;
            }

            if (m_importDescriptor)
            {
                result += 1; // FIXME calculate the imported symbols here!
            }
            return result;
        }

        SymbolsGen readSymbols() const override
        {
            ExportDirectoryPtr dir = m_exportDirectory;
            if (dir)  // Has exports
            {
                DWORD const * names = map<DWORD const *>(dir->AddressOfNames);
                DWORD const * addresses = map<DWORD const *>(dir->AddressOfNames);
                WORD const * ordinals = map<WORD const *>(dir->AddressOfNameOrdinals);

                for (DWORD i = 0; i < dir->NumberOfNames; ++i) 
                {
                    LPCCH mangledName = GUARD(map<char const *>(names[i]));
                    
                    co_yield detail::nameToSymbol(mangledName);
                }
            }

            ImportDescriptorPtr imp = m_importDescriptor;
            if (imp)  // Has imports
            {
                while (imp->OriginalFirstThunk)
                {
                    LPCCH importedModuleName = map<LPCCH>(imp->Name);

                    for (ThunkDataPtr namesTable = map<ThunkDataPtr>(imp->OriginalFirstThunk);
                         namesTable->u1.Function; ++namesTable)
                    {
                        Symbol symbol;
                        if (namesTable->u1.Ordinal & ImageOrdinalFlag)
                        {
                            String name = detail::toString(importedModuleName) + TEXT("/#") + 
                                detail::toString(namesTable->u1.Ordinal ^ ImageOrdinalFlag);
                            symbol.mangledName = symbol.demangledName = std::move(name);
                        }
                        else
                        {
                            ImportByNamePtr importByName = map<ImportByNamePtr>(
                                    namesTable->u1.AddressOfData);
                            LPCCH mangledName = reinterpret_cast<LPCCH>(importByName->Name);
                            // Avoiding extra copy of the Symbol instance
                            co_yield detail::nameToSymbol(mangledName);
                            continue;
                        }
                        symbol.implements = false;
                        co_yield std::move(symbol);
                    }
                    imp++;
                }
            }
        }

        ~PENativeSymbolReader()
        {
        }

    private:
        FileUPtr m_moduleFile;  // Whilst this ptr lives, memory mapping is valid
        BytePtr m_moduleBytes{};
        DOSHeaderPtr m_dosHeader{};
        NTHeadersPtr m_ntHeader{};
        SectionHeaderPtr m_sectionHeader{};
        WORD m_sectionsCount{};
        ExportDirectoryPtr m_exportDirectory{};
        ImportDescriptorPtr m_importDescriptor{};
    };
}

ISymbolReader::UPtr PENativeParser::reader(String const & imagePath) const
{
    // See https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format
    FileUPtr moduleFile = detail::createMappedFile(imagePath);

    GUARD(moduleFile);

    uint8_t const * mapped = moduleFile->map();

    LPCBYTE moduleBytes = reinterpret_cast<LPCBYTE>(mapped);

    // Checking for signatures
    IMAGE_DOS_HEADER const * dosHeader = reinterpret_cast<IMAGE_DOS_HEADER const *>(moduleBytes);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return {};
    }

    IMAGE_NT_HEADERS const * ntHeader = reinterpret_cast<IMAGE_NT_HEADERS const *>(moduleBytes + dosHeader->e_lfanew);

    WORD const machine = ntHeader->FileHeader.Machine;

    if (ntHeader->Signature != IMAGE_NT_SIGNATURE)
    {
        return {};
    }

    // Ignoring CLR/.NET assemblies
    if (ntHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        DWORD comDescrVA = reinterpret_cast<IMAGE_NT_HEADERS32 const *>(ntHeader)->OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress;
        if (comDescrVA)
        {
            return {};
        }
    }
    if (ntHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        DWORD comDescrVA = reinterpret_cast<IMAGE_NT_HEADERS64 const *>(ntHeader)->OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress;
        if (comDescrVA)
        {
            return {};
        }
    }

    using I386PEReader  = detail::PENativeSymbolReader<IMAGE_FILE_MACHINE_I386>;
    using AMD64PEReader = detail::PENativeSymbolReader<IMAGE_FILE_MACHINE_AMD64>;

    if (machine == IMAGE_FILE_MACHINE_I386)
    {
        return std::make_unique<I386PEReader>(std::move(moduleFile), moduleBytes);
    }

    else if (machine == IMAGE_FILE_MACHINE_AMD64)
    {
        return std::make_unique<AMD64PEReader>(std::move(moduleFile), moduleBytes);
    }

    return {};
}
