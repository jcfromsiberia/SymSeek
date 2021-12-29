module;

#include <symseek/Definitions.h>

export module symseek.interfaces.parser;

import <experimental/generator>;
import <functional>;
import <iterator>;
import <memory>;
import <string>;

import symseek.definitions;
import symseek.symbol;

export namespace SymSeek
{
    class ISymbolReader
    {
    public:
        using UPtr = std::unique_ptr<ISymbolReader>;

        using SymbolsGen = std::experimental::generator<RawSymbol>;

        virtual size_t symbolsCount() const = 0;  // for reserving enough space
        virtual SymbolsGen readSymbols() const = 0;

        virtual ~ISymbolReader() = default;
    };

    class IImageParser
    {
    public:
        using UPtr = std::unique_ptr<IImageParser>;

        virtual ISymbolReader::UPtr reader(String const & imagePath) const = 0;
        virtual ~IImageParser() = default;
    };
}
