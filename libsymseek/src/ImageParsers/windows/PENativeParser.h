#pragma once

#include <QtCore/QtGlobal>
#if !defined(Q_OS_WIN)
#   error Unsupported platform
#endif

#include <memory>
#include <type_traits>

#include <Windows.h>

#include <QtCore/QByteArray>

#include "src/ImageParsers/IImageParser.h"

namespace SymSeek
{
    class PENativeParser: public IImageParser
    {
    public:
        ISymbolReader::UPtr reader(QString imagePath) const override;
    };

    class PENativeSymbolReader: public ISymbolReader
    {
    public:
        PENativeSymbolReader(QByteArray moduleBytes);

        size_t symbolsCount() const override;
        void readInto(SymbolsInserter outputIter, SymbolHandler handler) const override;
        ~PENativeSymbolReader();
    private:
        QByteArray m_moduleByteArray;
        std::unique_ptr<struct PENativeSymbolReaderPrivate> m_priv;
    };
}
