#include "LIBNativeParser.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QtEndian>

#include <Debug.h>

#include "WinHelpers.h"

using namespace SymSeek;

ISymbolReader::UPtr LIBNativeParser::reader(QString imagePath) const
{
    auto archiveFile = std::make_unique<QFile>(imagePath);
    GUARD(archiveFile->open(QFile::ReadOnly));
    // Avoid reading the entire file into a QByteArray as it can be extremely huge.

    // See https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#archive-library-file-format
    if(archiveFile->read(8) != "!<arch>\n")
        return {};

    return std::make_unique<LIBNativeSymbolReader>(std::move(archiveFile));
}

LIBNativeSymbolReader::LIBNativeSymbolReader(std::unique_ptr<QFile> archiveFile)
: m_archiveFile{ std::move(archiveFile) }
{
    readSymbolsCount();
}

size_t LIBNativeSymbolReader::symbolsCount() const
{
    return m_symbolsCount;
}

void LIBNativeSymbolReader::readInto(ISymbolReader::SymbolsInserter outputIter, SymbolHandler handler) const
{
    if(!m_symbolsCount)
        return;
    quint64 const tableBeginOffset = /*Signature=*/8 + /*First_header=*/60 + /*Number_of_symbols=*/4 +
                             /*Offsets_array=*/4 * m_symbolsCount;
    m_archiveFile->seek(tableBeginOffset);
    // There is no obvious way to read null-terminated string from file,
    // QTextStream is unable to read this properly when the string's length is unknown

    // Find out the offset of the table end, it ends with \n
    m_archiveFile->readLine();
    quint64 tableEndOffset = m_archiveFile->pos();

    Q_ASSERT(tableBeginOffset < tableEndOffset);

    // Now we can map the symbol table onto the memory
    uchar * mapped = m_archiveFile->map(tableBeginOffset,
            tableEndOffset - tableBeginOffset, QFileDevice::MapPrivateOption);
    LPCCH symTable = reinterpret_cast<LPCCH>(mapped);
    for(uint32_t i = 0; i < m_symbolsCount; ++i, symTable += strlen(symTable) + /*terminator \0*/1)
    {
        *outputIter++ = detail::nameToSymbol(symTable);
    }
    m_archiveFile->unmap(mapped);
}

void LIBNativeSymbolReader::readSymbolsCount()
{
    Q_ASSERT(!!m_archiveFile);
    Q_ASSERT(m_archiveFile->isOpen());
    // Skip the 1st mem header https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#archive-member-headers
    m_archiveFile->seek(/*Signature=*/8 + /*First_header=*/60);

    // Number of symbols https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format#first-linker-member
    QByteArray symbolsCountBytes = m_archiveFile->read(4);

    // The number is in Big Endian.
    m_symbolsCount = qFromBigEndian<uint32_t>(symbolsCountBytes.data());
}
