#include "ELFNativeParser.h"

#include <QtCore/QFile>

#include <Debug.h>

using namespace SymSeek;

ISymbolReader::UPtr ELFNativeParser::reader(QString imagePath) const
{
    QFile elfFile(imagePath);
    GUARD(elfFile.open(QFile::ReadOnly));

    if(elfFile.read(SELFMAG) != ELFMAG)
    {
        return {};
    }

    Elf64_Ehdr elfHeader = { 0 };

    elfFile.read(reinterpret_cast<char*>(&elfHeader), sizeof(decltype(elfHeader)));

    detail::ElfSymHeaderArray elfSymbols{ new Elf64_Shdr[elfHeader.e_shnum] };

    elfFile.seek(elfHeader.e_shoff);
    elfFile.read(reinterpret_cast<char*>(elfSymbols.get()), elfHeader.e_shentsize * elfHeader.e_shnum);

    return std::make_unique<ELFNativeSymbolReader>(std::move(elfSymbols), elfHeader.e_shnum);
}

ELFNativeSymbolReader::ELFNativeSymbolReader(detail::ElfSymHeaderArray && elfSymbols, size_t count)
: m_elfSymbols{ std::move(elfSymbols) }
, m_count     { count                 }
{
}

size_t ELFNativeSymbolReader::symbolsCount() const
{
    return m_count;
}

void ELFNativeSymbolReader::readInto(ISymbolReader::SymbolsInserter outputIter, SymbolHandler handler) const
{

}
