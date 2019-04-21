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

    class COFFNativeParser: public IImageParser
    {
    public:
        ISymbolReader::UPtr reader(QString imagePath) const override;
    };
}
