#include <src/SymbolSeeker.h>

import <iterator>;

#include <QtCore/QCoreApplication>   // for qApp->processEvents()
#include <QtCore/QDirIterator>
#include <QtCore/QDebug>

import symseek;

using namespace SymSeek;
using namespace SymSeek::QtUI;

static QStringList findFilesByMasks(QString const & directoryPath, QStringList const & masks)
{
    QStringList result;
    QDirIterator iter(directoryPath, QDir::Dirs | QDir::NoDotAndDotDot |
                      QDir::Readable, QDirIterator::Subdirectories);
    while (iter.hasNext())
    {
        iter.next();
        result += findFilesByMasks(iter.filePath(), masks);
    }

    auto files = QDir(directoryPath).entryList(masks, QDir::Files | QDir::Readable | QDir::NoSymLinks);
    for (auto const & fileEntry: files)
    {
        result.append(QDir(directoryPath).filePath(fileEntry));
    }
    return result;
}

QVector<SymbolsInBinary> SymbolSeeker::findSymbols(
    QString const &directoryPath, QStringList const &masks, SymbolHandler handler)
{
    auto binaries = findFilesByMasks(directoryPath, masks);

    QVector<SymbolsInBinary> result;
    auto itemsCount = size_t(binaries.size());
    result.reserve(int(itemsCount));
    Q_EMIT startProcessingItems(itemsCount);

#ifdef Q_OS_WIN
    static IDemangler::UPtr const demanglers[] = 
    {
        createDemangler(Mangler::MSVC),
        createDemangler(Mangler::GCC)
    };
#else
#   error Not implemented
#endif

    // Could be run in parallel
    for (auto const & binary: binaries)
    {
        qApp->processEvents();
        if (m_interruptFlag)
        {
            m_interruptFlag = false;
            Q_EMIT interrupted();
            return result;
        }

        Q_EMIT itemStatus(binary, ProgressStatus::Start);

        if (ISymbolReader::UPtr reader = createReader(toString(binary)))
        {
            Symbols symbols;
            symbols.reserve(int(reader->symbolsCount()));
            // Payload
            for (auto && rawSymbol: reader->readSymbols())
            {
                std::optional<std::string> demangledName;
                for (auto const & demangler: demanglers)
                {
                    if (auto nameOpt = demangler->demangleName(rawSymbol.name.c_str()); nameOpt)
                    {
                        demangledName = nameOpt;
                        break;
                    }
                }

                Symbol symbol = demangledName.has_value() ? 
                                createSymbol(std::move(rawSymbol), demangledName.value()) :
                                Symbol{.raw = std::move(rawSymbol)};

                SymbolHandlerAction action = handler(symbol);
                if (action == SymbolHandlerAction::Skip)
                {
                    continue;
                }
                else if (action == SymbolHandlerAction::Stop)
                {
                    break;
                }
                Q_ASSERT(action == SymbolHandlerAction::Add);
                symbols.append(std::move(symbol));
            }
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
