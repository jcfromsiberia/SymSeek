#include <symseek/symseek.h>

#if SYMSEEK_OS_WIN()
#   include "src/ImageParsers/windows/LIBNativeParser.h"
#   include "src/ImageParsers/windows/PENativeParser.h"
#   include "src/ImageParsers/windows/COFFNativeParser.h"
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
}
