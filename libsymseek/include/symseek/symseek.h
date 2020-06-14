#pragma once

#include "IDemangler.h"
#include "IImageParser.h"

namespace SymSeek
{
    ISymbolReader::UPtr createReader(String const & imagePath);
    IDemangler::UPtr createDemangler(Mangler mangler);
    Symbol createSymbol(RawSymbol rawSymbol, std::string demangledName);
}
