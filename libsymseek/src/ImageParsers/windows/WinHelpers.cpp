#include "WinHelpers.h"

#include <memory>

#include <tchar.h>

#if defined(DBGHELP_FOUND)
#   include <dbghelp.h>
#endif

#if defined(PSAPI_FOUND)
#   include <psapi.h>
#endif

#if defined(__MINGW32__)
#   include <cxxabi.h>
#endif

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
        // 4) get `__unDNameEx` pointer from the module and invoke it.
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

namespace SymSeek::detail
{
    Symbol nameToSymbol(LPCCH mangledName, bool implements)
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
}
