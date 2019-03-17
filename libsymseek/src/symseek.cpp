#include <symseek/symseek.h>

#include <iterator>

#include <QtCore/QDirIterator>
#include <QtCore/QDebug>
#include <src/ImageParsers/IImageParser.h>


#if defined(Q_OS_WIN)
#   include "ImageParsers/COFFNativeParser.h"
#elif defined(Q_OS_LINUX)
#   include "ImageParsers/ELFNativeParser.h"
#endif

namespace SymSeek
{
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

    QVector<SymbolsInBinary> findSymbols(QString const &directoryPath,
            QStringList const &masks, SymbolHandler handler)
    {
        auto binaries = findFilesByMasks(directoryPath, masks);

        IImageParser::UPtr parsers[] = {
#if defined(Q_OS_WIN)
            std::make_unique<COFFNativeParser>(),
#elif defined(Q_OS_LINUX)
            std::make_unique<ELFNativeParser>(),
#endif
            {} // Sentinel to prevent worrying about the ending commas
        };

        QVector<SymbolsInBinary> result;
        result.reserve(binaries.size());

        // Could be run in parallel
        for(auto const & binary: binaries)
        {
            ISymbolReader::UPtr reader;
            for(auto const & parser: parsers)
            {
                if(!parser)
                    continue;
                reader = parser->reader(binary);
            }
            if(reader)
            {
                Symbols symbols;
                symbols.reserve(reader->symbolsCount());
                // Payload
                reader->readInto(std::back_inserter(symbols), handler);
                result.append({ binary, std::move(symbols) });
            }
        }

        return result;
    }
}