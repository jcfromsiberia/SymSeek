#pragma once

#include <functional>
#include <iterator>
#include <memory>

#include <QtCore/QString>
#include <QtCore/QVector>

#include <symseek/symseek.h>

namespace SymSeek
{
    class ISymbolReader
    {
    public:
        using UPtr = std::unique_ptr<ISymbolReader>;
        using SymbolsInserter = std::back_insert_iterator<Symbols>;

        virtual size_t symbolsCount() const = 0;  // for reserving enough space
        virtual void readInto(SymbolsInserter outputIter, SymbolHandler handler = {}) const = 0;

        virtual ~ISymbolReader() = default;
    };

    class IImageParser
    {
    public:
        using UPtr = std::unique_ptr<IImageParser>;

        virtual ISymbolReader::UPtr reader(QString imagePath) const = 0;
        virtual ~IImageParser() = default;
    };
}
