#include "COFFNativeParser.h"

#include <dbghelp.h>
#include <Windows.h>

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QRegExp>

#if defined(__MINGW32__)
#   include <cxxabi.h>
#endif

#include "Debug.h"

using namespace SymSeek;

// See https://stackoverflow.com/questions/45420985/c-get-native-dll-dependencies-without-loading-it-in-process
ISymbolReader::UPtr COFFNativeParser::reader(QString imagePath) const
{
    // See https://upload.wikimedia.org/wikipedia/commons/7/70/Portable_Executable_32_bit_Structure_in_SVG.svg
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
    if(ntHeader->Signature != IMAGE_NT_SIGNATURE && ntHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
    {
        return {};
    }

    return std::make_unique<COFFNativeSymbolReader>(std::move(moduleByteArray));
}

static Symbol nameToSymbol(LPCCH mangledName, bool implements = true)
{
    CHAR demandledSymbol[8192] = {0};  // should be enough
    // Unfortunately this function doesn't handle move semantics,
    // see e.g https://github.com/lucasg/Dependencies/issues/32
    ::UnDecorateSymbolName(mangledName, demandledSymbol, sizeof(demandledSymbol),
                           UNDNAME_COMPLETE | UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_LEADING_UNDERSCORES);
    QString mangledNameStr = QString::fromLatin1(mangledName);
    QString demangledNameStr = QString::fromLatin1(demandledSymbol);
#if defined(__MINGW32__)
    if(demangledNameStr == mangledNameStr && mangledNameStr.startsWith("_Z"))  // It's likely to be GCC mangling
    {
        int status{};
        char * realName = ::abi::__cxa_demangle(mangledName, /*output_buffer=*/nullptr, /*length*/nullptr, &status);
        if(!status)
        {
            demangledNameStr = QString::fromLatin1(realName);
            ::free(realName);
        }
    }
#endif

    Symbol result;
    result.implements = implements;
    result.mangledName = mangledNameStr;
    result.demangledName = demangledNameStr;
    QString name = demangledNameStr.trimmed();

    if(!name.contains(' '))
    {
        // C function
        return result;
    }

    static QRegExp const constRx{ "^.+\\W+(\\s*const)$" };
    if(int index = constRx.indexIn(name); index > -1)
    {
        result.modifiers |= Symbol::IsConst;
        result.type = NameType::Method;
        name.chop(constRx.cap(1).length());
    }

    static QRegExp const accessModifierRx{ "^(public|protected|private):" };
    if(accessModifierRx.indexIn(name) > -1)
    {
        result.type = NameType::Method;
        QString const accessStr = accessModifierRx.cap(1);
        result.access = Access::Public;
        if(accessStr == "protected")
        {
            result.access = Access::Protected;
        }
        else if(accessStr == "private")
        {
            result.access = Access::Private;
        }
        name.remove(0, accessStr.length() + 2 /*colon and space*/);
    }

    static QRegExp const modifierRx{ "^(virtual|static)" };
    if(result.type == NameType::Method && modifierRx.indexIn(name) > -1)
    {
        QString const modifier = modifierRx.cap(1);
        if(modifier == "static")
            result.modifiers |= Symbol::IsStatic;
        else
            result.modifiers |= Symbol::IsVirtual;
        name.remove(0, modifier.length() + 1 /*space*/);
    }

    static QRegExp const signatureRx{ R"(^(.+)\((.*)\)$)" };
    if(!signatureRx.exactMatch(name))
    {
        result.type = NameType::Variable;
    }

    result.demangledName = name;

    return result;
}

namespace SymSeek
{
    struct COFFNativeSymbolReaderPrivate
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

COFFNativeSymbolReader::COFFNativeSymbolReader(QByteArray && moduleByteArray)
: m_moduleByteArray{ std::move(moduleByteArray) }
, m_priv{ std::make_unique<COFFNativeSymbolReaderPrivate>() }
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

size_t COFFNativeSymbolReader::symbolsCount() const
{
    size_t result{};
    if(m_priv->exportDirectory)
        result += m_priv->exportDirectory->NumberOfNames;
    if(m_priv->importDescriptor)
        result += 1; // FIXME calculate the imported symbols here!
    return result;
}

void COFFNativeSymbolReader::readInto(SymbolsInserter outputIter, SymbolHandler handler) const
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
                *outputIter++ = nameToSymbol(mangledName);
                continue;
            }
            Symbol symbol = nameToSymbol(mangledName);
            SymbolHandlerAction action = handler(symbol);
            if(action == SymbolHandlerAction::Skip)
                continue;
            if(action == SymbolHandlerAction::Stop)
                return;
            Q_ASSERT(action == SymbolHandlerAction::Add);
            *outputIter++ = nameToSymbol(mangledName);
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
                        *outputIter++ = nameToSymbol(mangledName, /*implements=*/false);
                        continue;
                    }
                    symbol = nameToSymbol(mangledName);
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

COFFNativeSymbolReader::~COFFNativeSymbolReader()
{
}
