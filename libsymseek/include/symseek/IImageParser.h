#pragma once

#include <experimental/generator>
#include <functional>
#include <iterator>
#include <memory>
#include <string>

#include "Definitions.h"
#include "Symbol.h"

namespace SymSeek
{
    class ISymbolReader
    {
    public:
        using UPtr = std::unique_ptr<ISymbolReader>;
        using SymbolsGen = std::experimental::generator<Symbol>;

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
