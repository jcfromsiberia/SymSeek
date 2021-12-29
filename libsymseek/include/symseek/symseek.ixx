module;

export module symseek;

export import symseek.definitions;
export import symseek.interfaces.demangler;
export import symseek.interfaces.parser;
export import symseek.symbol;

export namespace SymSeek
{
    ISymbolReader::UPtr createReader(String const & imagePath);
    IDemangler::UPtr createDemangler(Mangler mangler);
    Symbol createSymbol(RawSymbol rawSymbol, std::string demangledName);
}
