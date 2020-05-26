#pragma once

#include "IImageParser.h"

namespace SymSeek
{
    ISymbolReader::UPtr createReader(String const & imagePath);
}
