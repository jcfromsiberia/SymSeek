#include "COFFNativeParser.h"

#include <algorithm>

#include <Windows.h>
#include <tchar.h>

#if defined(DBGHELP_FOUND)
#   include <dbghelp.h>
#endif

#if defined(PSAPI_FOUND)
#   include <psapi.h>
#endif

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

static QString demangleName(LPCCH mangledName)
{
    // Undocumented crap. The only doc I found is https://source.winehq.org/WineAPI/__unDNameEx.html
    using MallocFuncPtr = void * (*)(size_t);
    using FreeFuncPtr   = void (*)(void *);
    using UndNamePtr = char * (*)(
            CHAR * buffer,
            LPCCH mangled,
            int buflen,
            MallocFuncPtr,
            FreeFuncPtr,
            void * reserved,
            unsigned short int flags);

    // Trying to find the advanced undecorate function
    auto findUndName = [](TCHAR const * prefixName, TCHAR const * dllName)
    {
        HMODULE vcRuntimeModule{};
#if defined(PSAPI_FOUND)
        // 1) scan all modules loaded;
        // 2) find the required one by name;
        // 3) if not found, try to load the system-wide;
        // 3) get `__unDNameEx` pointer from the module and invoke it.
        DWORD requiredBytes{};
        HANDLE currentProcess = GetCurrentProcess();
        // Cannot GetModuleHandle for the VC runtime library,
        // see http://alax.info/blog/1155
        EnumProcessModules(currentProcess, nullptr, 0, &requiredBytes);
        Q_ASSERT(requiredBytes);
        DWORD modulesCount = requiredBytes / sizeof(HMODULE);
        auto handles = std::make_unique<HMODULE[]>(modulesCount);
        if(!EnumProcessModules(currentProcess, handles.get(), requiredBytes, &requiredBytes))
            return UndNamePtr(nullptr);

        for (DWORD i = 0; i < modulesCount; ++i)
        {
            HMODULE module = handles[i];
            Q_ASSERT(module);
            TCHAR buffer[4096] = {0};
            GetModuleFileName(module, buffer, sizeof(buffer) / sizeof(TCHAR));
            _tcslwr_s(buffer, sizeof(buffer) / sizeof(TCHAR));
            TCHAR const * baseName = std::find(std::rbegin(buffer), std::rend(buffer), TEXT('\\')).base();
            if (_tcsstr(baseName, prefixName))
            {
                vcRuntimeModule = module;
                break;
            }
        }
#endif  //defined(PSAPI_FOUND)
        if(!vcRuntimeModule)
        {
            // If the control flow is here, it means using a non-MSVC toolchain.
            // Trying to load the vcruntime manually
            vcRuntimeModule = LoadLibrary(dllName);
            // No FreeLibrary call as the runtime should live while the static __unDNameEx exists.
            // Don't find it as a resource leak.
        }
        return reinterpret_cast<UndNamePtr>(vcRuntimeModule ? GetProcAddress(vcRuntimeModule, "__unDNameEx") : nullptr);
    };

    static UndNamePtr __unDNameEx = [&findUndName]() {
        // msvcrt has the obsolete __unDNameEx which doesn't handle move semantics
        // That's why looking for vcruntime first.
        auto result = findUndName(TEXT("vcruntime"), TEXT("vcruntime140.dll"));
        if(result)
            return result;
        return findUndName(TEXT("msvcrt"), TEXT("msvcrt.dll"));
    }();

    QString result = QString::fromLatin1(mangledName);

    // Prior to undecorating, check if the name has been mangled with the MSVC mangler
    if(result.startsWith('?'))
    {
        CHAR demandledSymbol[8192] = {0};  // should be enough
        auto const undFlags = UNDNAME_COMPLETE | UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_LEADING_UNDERSCORES;
        if(__unDNameEx)
        {
            __unDNameEx(demandledSymbol, mangledName,
                        sizeof(demandledSymbol) / sizeof(CHAR), ::malloc, ::free, /*reserved=*/nullptr, undFlags);
            result = QString::fromLatin1(demandledSymbol);
        }
#if defined(DBGHELP_FOUND)
        else
        {
            // Unfortunately this function doesn't handle move semantics,
            // see e.g https://github.com/lucasg/Dependencies/issues/32
            ::UnDecorateSymbolName(mangledName, demandledSymbol, sizeof(demandledSymbol), undFlags);
            result = QString::fromLatin1(demandledSymbol);
        }
#endif  //defined(DBGHELP_FOUND)
    }
#if defined(__MINGW32__)
    else if(result.startsWith("_Z"))  // or the GCC mangler
    {
        int status{};
        char * realName = ::abi::__cxa_demangle(mangledName, /*output_buffer=*/nullptr, /*length*/nullptr, &status);
        if(!status)
        {
            result = QString::fromLatin1(realName);
            ::free(realName);
        }
    }
#endif  //defined(__MINGW32__)

    return result;
}

static Symbol nameToSymbol(LPCCH mangledName, bool implements = true)
{
    QString mangledNameStr = QString::fromLatin1(mangledName);
    QString demangledNameStr = demangleName(mangledName);

    Symbol result;
    result.implements = implements;
    result.mangledName = mangledNameStr;
    result.demangledName = demangledNameStr;

    if(mangledNameStr == demangledNameStr)
    {
        // C function
        return result;
    }
    QString name = demangledNameStr.trimmed();

    static QRegExp const constRx{ R"(^.+\W+\s*const\s*(&|&&)?$)" };
    if(int index = constRx.indexIn(name); index > -1)
    {
        result.modifiers |= Symbol::IsConst;
        result.type = NameType::Method;
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

    static QRegExp const signatureRx{ R"(^(.+)\((.*)\)(\s*const\s*)?(&|&&)?$)" };
    if(!signatureRx.exactMatch(name))
    {
        result.type = NameType::Variable;
        if(name.contains("const "))
            result.modifiers |= Symbol::IsConst;
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
