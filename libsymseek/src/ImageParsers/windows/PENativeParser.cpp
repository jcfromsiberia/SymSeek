#include "PENativeParser.h"

#include <algorithm>

#include <Windows.h>

#include <QtCore/QFile>
#include <QtCore/QRegExp>

#include <Debug.h>

#include "WinHelpers.h"

using namespace SymSeek;

ISymbolReader::UPtr PENativeParser::reader(QString imagePath) const
{
    // See https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format
    QFile moduleFile(imagePath);
    GUARD(moduleFile.open(QFile::ReadOnly));

    QByteArray moduleByteArray = moduleFile.readAll();
    LPBYTE moduleBytes = reinterpret_cast<LPBYTE>(moduleByteArray.data());

    // Checking for signatures
    PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(moduleBytes);
    if(dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return {};
    }

    PIMAGE_NT_HEADERS ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(moduleBytes + dosHeader->e_lfanew);
    if(ntHeader->Signature != IMAGE_NT_SIGNATURE/* && ntHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64*/)
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

    return std::make_unique<PENativeSymbolReader>(std::move(moduleByteArray));
}

namespace SymSeek
{
    struct PENativeSymbolReaderPrivate
    {
        LPBYTE moduleBytes{};
        PIMAGE_DOS_HEADER dosHeader{};
        PIMAGE_NT_HEADERS ntHeader{};
        PIMAGE_SECTION_HEADER sectionHeader{};
        WORD sectionsCount{};
        PIMAGE_EXPORT_DIRECTORY exportDirectory{};
        PIMAGE_IMPORT_DESCRIPTOR importDescriptor{};

        template<typename T>
        T map(DWORD virtualAddress) const
        {
            DWORD section{};
            for (; section < sectionsCount; ++section)
            {
                DWORD sectionBeginRVA = GUARD(sectionHeader)[section].VirtualAddress;
                DWORD sectionEndRVA = sectionBeginRVA + sectionHeader[section].Misc.VirtualSize;
                if (sectionBeginRVA <= virtualAddress && virtualAddress <= sectionEndRVA)
                    break;
            }
            DWORD offset = GUARD(sectionHeader)[section].PointerToRawData +
                    virtualAddress - sectionHeader[section].VirtualAddress;
            return reinterpret_cast<T>(GUARD(moduleBytes) + offset);
        }
    };
}

PENativeSymbolReader::PENativeSymbolReader(QByteArray moduleByteArray)
: m_moduleByteArray{ std::move(moduleByteArray) }
, m_priv{ std::make_unique<PENativeSymbolReaderPrivate>() }
{
    m_priv->moduleBytes = reinterpret_cast<LPBYTE>(m_moduleByteArray.data());
    m_priv->dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(m_priv->moduleBytes);
    m_priv->ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(m_priv->moduleBytes + m_priv->dosHeader->e_lfanew);

    m_priv->sectionsCount = m_priv->ntHeader->FileHeader.NumberOfSections;
    m_priv->sectionHeader = reinterpret_cast<PIMAGE_SECTION_HEADER>(
            m_priv->moduleBytes + m_priv->dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS));

    DWORD exportAddressOffset = m_priv->ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if(exportAddressOffset)
    {
        m_priv->exportDirectory = m_priv->map<PIMAGE_EXPORT_DIRECTORY>(exportAddressOffset);
    }

    DWORD importAddressOffset = m_priv->ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if(importAddressOffset)
    {
        m_priv->importDescriptor = m_priv->map<PIMAGE_IMPORT_DESCRIPTOR>(importAddressOffset);
    }
}

size_t PENativeSymbolReader::symbolsCount() const
{
    size_t result{};
    if(m_priv->exportDirectory)
        result += m_priv->exportDirectory->NumberOfNames;
    if(m_priv->importDescriptor)
        result += 1; // FIXME calculate the imported symbols here!
    return result;
}

void PENativeSymbolReader::readInto(SymbolsInserter outputIter, SymbolHandler handler) const
{
    PIMAGE_EXPORT_DIRECTORY dir = m_priv->exportDirectory;
    if(dir)  // Has exports
    {
        PDWORD names = m_priv->map<PDWORD>(dir->AddressOfNames);
        PDWORD addresses = m_priv->map<PDWORD>(dir->AddressOfNames);
        PWORD ordinals = m_priv->map<PWORD>(dir->AddressOfNameOrdinals);

        for (DWORD i = 0; i < dir->NumberOfNames; ++i) {
            LPCCH mangledName = GUARD(m_priv->map < char const *>(names[i]));
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
            *outputIter++ = detail::nameToSymbol(mangledName);
        }
    }

    PIMAGE_IMPORT_DESCRIPTOR imp = m_priv->importDescriptor;
    if(imp)  // Has imports
    {
        while(imp->OriginalFirstThunk)
        {
            LPCCH importedModuleName = m_priv->map<LPCCH>(imp->Name);

            for(PIMAGE_THUNK_DATA namesTable = m_priv->map<PIMAGE_THUNK_DATA>(imp->OriginalFirstThunk);
                namesTable->u1.Function; ++namesTable)
            {
                Symbol symbol;
                if(namesTable->u1.Ordinal & IMAGE_ORDINAL_FLAG)
                {
                    QString name = QStringLiteral("%1/#%2").arg(importedModuleName).arg(namesTable->u1.Ordinal);
                    symbol.mangledName = symbol.demangledName = name;
                }
                else
                {
                    PIMAGE_IMPORT_BY_NAME importByName = m_priv->map<PIMAGE_IMPORT_BY_NAME>(
                            namesTable->u1.AddressOfData);
                    LPCCH mangledName = reinterpret_cast<LPCCH>(importByName->Name);
                    if (!handler)
                    {
                        // Avoiding extra copy of the Symbol instance
                        *outputIter++ = detail::nameToSymbol(mangledName, /*implements=*/false);
                        continue;
                    }
                    symbol = detail::nameToSymbol(mangledName);
                }
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

PENativeSymbolReader::~PENativeSymbolReader()
{
}