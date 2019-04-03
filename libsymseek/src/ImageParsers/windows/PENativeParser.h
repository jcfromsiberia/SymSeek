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
}
