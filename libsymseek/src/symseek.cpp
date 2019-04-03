#include <symseek/symseek.h>

#include <iterator>

#include <QtCore/QCoreApplication>   // for qApp->processEvents()
#include <QtCore/QDirIterator>
#include <QtCore/QDebug>

#include "ImageParsers/IImageParser.h"
#if defined(Q_OS_WIN)
#   include "src/ImageParsers/windows/LIBNativeParser.h"
#   include "src/ImageParsers/windows/PENativeParser.h"
#elif defined(Q_OS_LINUX)
#   include "ImageParsers/linux/ELFNativeParser.h"
#endif

using namespace SymSeek;

static QStringList findFilesByMasks(QString const & directoryPath, QStringList const & masks)
{
    QStringList result;
    QDirIterator iter(directoryPath, QDir::Dirs | QDir::NoDotAndDotDot |
                      QDir::Readable, QDirIterator::Subdirectories);
    while(iter.hasNext())
    {
        iter.next();
        result += findFilesByMasks(iter.filePath(), masks);
    }

    auto files = QDir(directoryPath).entryList(masks, QDir::Files | QDir::Readable | QDir::NoSymLinks);
    for(auto const & fileEntry: files)
    {
        result.append(QDir(directoryPath).filePath(fileEntry));
    }
    return result;
}

QVector<SymbolsInBinary> SymbolSeeker::findSymbols(QString const &directoryPath,
        QStringList const &masks, SymbolHandler handler)
{
    auto binaries = findFilesByMasks(directoryPath, masks);

    static IImageParser::UPtr parsers[] = {
#if defined(Q_OS_WIN)
        std::make_unique<LIBNativeParser>(),
        std::make_unique<PENativeParser>(),
#elif defined(Q_OS_LINUX)
        std::make_unique<ELFNativeParser>(),
#endif
    };

    QVector<SymbolsInBinary> result;
    auto itemsCount = size_t(binaries.size());
    result.reserve(int(itemsCount));
    Q_EMIT startProcessingItems(itemsCount);

    // Could be run in parallel
    for(auto const & binary: binaries)
    {
        qApp->processEvents();
        if(m_interruptFlag)
        {
            m_interruptFlag = false;
            Q_EMIT interrupted();
            return result;
        }

        Q_EMIT itemStatus(binary, ProgressStatus::Start);
        ISymbolReader::UPtr reader;
        for(auto const & parser: parsers)
        {
            if(reader)
                break;
            reader = parser->reader(binary);
        }
        if(reader)
        {
            Symbols symbols;
            symbols.reserve(int(reader->symbolsCount()));
            // Payload
            reader->readInto(std::back_inserter(symbols), handler);
            result.append({ binary, std::move(symbols) });
            Q_EMIT itemStatus(binary, ProgressStatus::Finish);
        }
        else
        {
            Q_EMIT itemStatus(binary, ProgressStatus::Reject);
        }

        Q_EMIT itemsRemaining(--itemsCount);
    }

    return result;
}

void SymbolSeeker::interrupt()
{
    m_interruptFlag = true;
}
