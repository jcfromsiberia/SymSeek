#pragma once

// Even though the parser doesn't rely on WinAPI, it is using the win-specific demangler ;(
#include <QtCore/QtGlobal>
#if !defined(Q_OS_WIN)
#   error Unsupported platform
#endif

#include <memory>

#include <QtCore/QFile>

#include "src/ImageParsers/IImageParser.h"

namespace SymSeek
{
    class LIBNativeParser : public IImageParser
    {
    public:
        ISymbolReader::UPtr reader(QString imagePath) const override;
    };

    class LIBNativeSymbolReader : public ISymbolReader
    {
    public:
        LIBNativeSymbolReader(std::unique_ptr<QFile> archiveFile);

        size_t symbolsCount() const override;

        void readInto(SymbolsInserter outputIter, SymbolHandler handler) const override;

    private:
        void readSymbolsCount();

    private:
        // QFile is not movable :(
        std::unique_ptr<QFile> m_archiveFile;
        uint32_t m_symbolsCount{};
    };
}
