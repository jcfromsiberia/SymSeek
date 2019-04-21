#include "PENativeParser.h"

#include <algorithm>
#include <type_traits>

#include <Windows.h>

#include <QtCore/QFile>
#include <QtCore/QRegExp>

#include <Debug.h>

#include "WinHelpers.h"

using namespace SymSeek;

template<WORD Machine>
struct PETypes;

template<>
struct PETypes<IMAGE_FILE_MACHINE_I386>
{
    using BytePtr             = LPBYTE;
    using DOSHeaderPtr        = PIMAGE_DOS_HEADER;
    using ExportDirectoryPtr  = PIMAGE_EXPORT_DIRECTORY;
    using ImportByNamePtr     = PIMAGE_IMPORT_BY_NAME;
    using ImportDescriptorPtr = PIMAGE_IMPORT_DESCRIPTOR;
    using NTHeaders           = IMAGE_NT_HEADERS32;
    using NTHeadersPtr        = PIMAGE_NT_HEADERS32;
    using OptionalHeaderPtr   = PIMAGE_OPTIONAL_HEADER32;
    using SectionHeaderPtr    = PIMAGE_SECTION_HEADER;
    using ThunkDataPtr        = PIMAGE_THUNK_DATA32;

    static constexpr auto ImageOrdinalFlag = IMAGE_ORDINAL_FLAG32;
};

template<>
struct PETypes<IMAGE_FILE_MACHINE_AMD64>
{
    using BytePtr             = LPBYTE;
    using DOSHeaderPtr        = PIMAGE_DOS_HEADER;
    using ExportDirectoryPtr  = PIMAGE_EXPORT_DIRECTORY;
    using ImportByNamePtr     = PIMAGE_IMPORT_BY_NAME;
    using ImportDescriptorPtr = PIMAGE_IMPORT_DESCRIPTOR;
    using NTHeaders           = IMAGE_NT_HEADERS64;
    using NTHeadersPtr        = PIMAGE_NT_HEADERS64;
    using OptionalHeaderPtr   = PIMAGE_OPTIONAL_HEADER64;
    using SectionHeaderPtr    = PIMAGE_SECTION_HEADER;
    using ThunkDataPtr        = PIMAGE_THUNK_DATA64;

    static constexpr auto ImageOrdinalFlag = IMAGE_ORDINAL_FLAG64;
};

using QFileUPtr = std::unique_ptr<QFile>;

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
        T map(DWORD virtualAddress) const
        {
            DWORD section{};
            for (; section < m_sectionsCount; ++section)
            {
                DWORD sectionBeginRVA = GUARD(m_sectionHeader)[section].VirtualAddress;
                DWORD sectionEndRVA = sectionBeginRVA + m_sectionHeader[section].Misc.VirtualSize;
                if (sectionBeginRVA <= virtualAddress && virtualAddress <= sectionEndRVA)
                    break;
            }
            DWORD offset = GUARD(m_sectionHeader)[section].PointerToRawData +
                           virtualAddress - m_sectionHeader[section].VirtualAddress;
            return reinterpret_cast<T>(GUARD(m_moduleBytes) + offset);
        }
    public:
        PENativeSymbolReader(QFileUPtr moduleFile, BytePtr moduleBytes)
        : m_moduleFile { std::move(moduleFile) }
        , m_moduleBytes{ moduleBytes           }
        {
            m_dosHeader = reinterpret_cast<DOSHeaderPtr>(m_moduleBytes);
            m_ntHeader = reinterpret_cast<NTHeadersPtr>(m_moduleBytes + m_dosHeader->e_lfanew);

            m_sectionsCount = m_ntHeader->FileHeader.NumberOfSections;
            m_sectionHeader = reinterpret_cast<SectionHeaderPtr>(
                    m_moduleBytes + m_dosHeader->e_lfanew + sizeof(std::remove_pointer_t<NTHeadersPtr>));

            if(DWORD exportAddressOffset = m_ntHeader->
                    OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress; exportAddressOffset)
            {
                m_exportDirectory = map<ExportDirectoryPtr>(exportAddressOffset);
            }

            if(DWORD importAddressOffset = m_ntHeader->
                    OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress; importAddressOffset)
            {
                m_importDescriptor = map<ImportDescriptorPtr>(importAddressOffset);
            }
        }

        size_t symbolsCount() const override
        {
            size_t result{};
            if(m_exportDirectory)
                result += m_exportDirectory->NumberOfNames;
            if(m_importDescriptor)
                result += 1; // FIXME calculate the imported symbols here!
            return result;
        }

        void readInto(SymbolsInserter outputIter, SymbolHandler handler) const override
        {
            ExportDirectoryPtr dir = m_exportDirectory;
            if(dir)  // Has exports
            {
                PDWORD names = map<PDWORD>(dir->AddressOfNames);
                PDWORD addresses = map<PDWORD>(dir->AddressOfNames);
                PWORD ordinals = map<PWORD>(dir->AddressOfNameOrdinals);

                for (DWORD i = 0; i < dir->NumberOfNames; ++i) {
                    LPCCH mangledName = GUARD(map < char const *>(names[i]));
                    if(!handler)
                    {
                        // Avoiding extra copy of the Symbol instance
                        *outputIter++ = detail::nameToSymbol(mangledName);
                        continue;
                    }
                    Symbol symbol = detail::nameToSymbol(mangledName);
                    SymbolHandlerAction action = handler(symbol);
                    if(action == SymbolHandlerAction::Skip)
                        continue;
                    if(action == SymbolHandlerAction::Stop)
                        return;
                    Q_ASSERT(action == SymbolHandlerAction::Add);
                    *outputIter++ = std::move(symbol);
                }
            }

            ImportDescriptorPtr imp = m_importDescriptor;
            if(imp)  // Has imports
            {
                while(imp->OriginalFirstThunk)
                {
                    LPCCH importedModuleName = map<LPCCH>(imp->Name);

                    for(ThunkDataPtr namesTable = map<ThunkDataPtr>(imp->OriginalFirstThunk);
                        namesTable->u1.Function; ++namesTable)
                    {
                        Symbol symbol;
                        if(namesTable->u1.Ordinal & ImageOrdinalFlag)
                        {
                            QString name = QStringLiteral("%1/#%2").arg(importedModuleName)
                                    .arg(namesTable->u1.Ordinal ^ ImageOrdinalFlag);
                            symbol.mangledName = symbol.demangledName = name;
                        }
                        else
                        {
                            ImportByNamePtr importByName = map<ImportByNamePtr>(
                                    namesTable->u1.AddressOfData);
                            LPCCH mangledName = reinterpret_cast<LPCCH>(importByName->Name);
                            if (!handler)
                            {
                                // Avoiding extra copy of the Symbol instance
                                *outputIter++ = detail::nameToSymbol(mangledName);
                                continue;
                            }
                            symbol = detail::nameToSymbol(mangledName);
                        }
                        symbol.implements = false;
                        SymbolHandlerAction action = handler(symbol);
                        if(action == SymbolHandlerAction::Skip)
                            continue;
                        if(action == SymbolHandlerAction::Stop)
                            return;
                        Q_ASSERT(action == SymbolHandlerAction::Add);
                        *outputIter++ = std::move(symbol);
                    }
                    imp++;
                }
            }
        }
        ~PENativeSymbolReader()
        {
        }

    private:
        QFileUPtr m_moduleFile;  // Whilst this ptr lives, memory mapping is valid
        BytePtr m_moduleBytes{};
        DOSHeaderPtr m_dosHeader{};
        NTHeadersPtr m_ntHeader{};
        SectionHeaderPtr m_sectionHeader{};
        WORD m_sectionsCount{};
        ExportDirectoryPtr m_exportDirectory{};
        ImportDescriptorPtr m_importDescriptor{};
    };
}

ISymbolReader::UPtr PENativeParser::reader(QString imagePath) const
{
    // See https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format
    QFileUPtr moduleFile{std::make_unique<QFile>(imagePath)};
    GUARD(moduleFile->open(QFile::ReadOnly));

    uchar * mapped = moduleFile->map(0, moduleFile->size(), QFileDevice::MapPrivateOption);

    LPBYTE moduleBytes = reinterpret_cast<LPBYTE>(mapped);

    // Checking for signatures
    PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(moduleBytes);
    if(dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return {};
    }

    PIMAGE_NT_HEADERS ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(moduleBytes + dosHeader->e_lfanew);

    WORD const machine = ntHeader->FileHeader.Machine;

    if(ntHeader->Signature != IMAGE_NT_SIGNATURE)
    {
        return {};
    }

    // Ignoring CLR/.NET assemblies
    if(ntHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        DWORD comDescrVA = reinterpret_cast<PIMAGE_NT_HEADERS32>(ntHeader)->OptionalHeader
                .DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress;
        if(comDescrVA)
            return {};
    }
    if(ntHeader->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        DWORD comDescrVA = reinterpret_cast<PIMAGE_NT_HEADERS64>(ntHeader)->OptionalHeader
                .DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress;
        if(comDescrVA)
            return {};
    }

    using I386PEReader  = detail::PENativeSymbolReader<IMAGE_FILE_MACHINE_I386>;
    using AMD64PEReader = detail::PENativeSymbolReader<IMAGE_FILE_MACHINE_AMD64>;

    if(machine == IMAGE_FILE_MACHINE_I386)
        return std::make_unique<I386PEReader>(std::move(moduleFile), moduleBytes);
    if(machine == IMAGE_FILE_MACHINE_AMD64)
        return std::make_unique<AMD64PEReader>(std::move(moduleFile), moduleBytes);

    return {};
}
