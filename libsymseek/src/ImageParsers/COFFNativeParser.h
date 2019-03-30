#pragma once

#include <memory>
#include <type_traits>

#include <Windows.h>

#include <QtCore/QByteArray>

#include "IImageParser.h"

namespace SymSeek
{
    class COFFNativeParser: public IImageParser
    {
    public:
        ISymbolReader::UPtr reader(QString imagePath) const override;
    };

    class COFFNativeSymbolReader: public ISymbolReader
    {
    public:
        COFFNativeSymbolReader(QByteArray && moduleBytes);

        size_t symbolsCount() const override;
        void readInto(SymbolsInserter outputIter, SymbolHandler handler) const override;
        ~COFFNativeSymbolReader();
    private:
        QByteArray m_moduleByteArray;
        std::unique_ptr<struct COFFNativeSymbolReaderPrivate> m_priv;
    };
}
