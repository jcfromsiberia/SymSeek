#include <symseek/symseek.h>

#include <regex>

#if SYMSEEK_OS_WIN()
#   include "src/Demanglers/windows/GCCDemangler.h"
#   include "src/Demanglers/windows/MSVCDemangler.h" 

#   include "src/ImageParsers/windows/COFFNativeParser.h"
#   include "src/ImageParsers/windows/LIBNativeParser.h"
#   include "src/ImageParsers/windows/PENativeParser.h"
#elif SYMSEEK_OS_LIN()
#   include "ImageParsers/linux/ELFNativeParser.h"
#endif

namespace SymSeek
{
    ISymbolReader::UPtr createReader(String const & imagePath)
    {
        static IImageParser::UPtr const parsers[] = {
#if SYMSEEK_OS_WIN()
            std::make_unique<LIBNativeParser>(),
            std::make_unique<PENativeParser>(),
            std::make_unique<COFFNativeParser>(),
#elif SYMSEEK_OS_LIN()
            std::make_unique<ELFNativeParser>(),
#endif
        };

        for (auto const & parser: parsers)
        {
            if (auto reader = parser->reader(imagePath))
            {
                return reader;
            }
        }
        return {};
    }

    IDemangler::UPtr createDemangler(Mangler mangler)
    {
        switch (mangler)
        {
            case Mangler::MSVC:
                return std::make_unique<MSVCDemangler>();
            case Mangler::GCC:
                return std::make_unique<GCCDemangler>();
        }
        return {};
    }

    Symbol createSymbol(RawSymbol rawSymbol, std::string demangledName)
    {
        std::string name = demangledName;
        Symbol result;
        result.raw = std::move(rawSymbol);
        
        static std::regex const constRx{
            R"(^.+\W+\s*const\s*(&|&&)?$)", std::regex::optimize};
        if (std::smatch match; std::regex_match(name, match, constRx))
        {
            result.modifiers |= Symbol::IsConst;
            result.type = NameType::Method;
        }

        static std::regex const accessModifierRx{
            R"(^(public|protected|private):.+)", std::regex::optimize};
        if (std::smatch match; std::regex_match(name, match, accessModifierRx))
        {
            result.type = NameType::Method;
            result.access = Access::Public;
            String const & accessStr = match[1];

            if (accessStr == "protected")
            {
                result.access = Access::Protected;
            }
            else if (accessStr == "private")
            {
                result.access = Access::Private;
            }

            name.erase(0, accessStr.length() + 2 /*colon and space*/);
        }

        static std::regex const modifierRx{
            R"(^(virtual|static).+)", std::regex::optimize};
        if (std::smatch match; std::regex_match(name, match, modifierRx))
        {
            String const & modifier = match[1];
            if (modifier == "static")
            {
                result.modifiers |= Symbol::IsStatic;
            }
            else
            {
                result.modifiers |= Symbol::IsVirtual;
            }

            name.erase(0, modifier.length() + 1/*space*/);
        }

        static std::regex const signatureRx{
             R"(^(.+)\((.*)\)(\s*const\s*)?(&|&&)?$)", std::regex::optimize};
        if (std::smatch match; !std::regex_match(name, match, signatureRx))
        {
            result.type = NameType::Variable;
            if (name.find("const ") != String::npos)
            {
                result.modifiers |= Symbol::IsConst;
            }
        }

        result.demangledName = std::move(demangledName);

        return result;
    }
}
