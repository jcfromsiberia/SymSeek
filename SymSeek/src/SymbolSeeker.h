#pragma once

#include <QtCore/QObject>

import symseek.symbol;

namespace SymSeek::QtUI
{
    enum class SymbolHandlerAction: uint8_t
    {
        // Add the symbol to the result
        Add,

        // Skip the symbol,
        Skip,

        // Stop processing the binary
        Stop
    };

    using SymbolHandler = std::function<SymbolHandlerAction(SymSeek::Symbol const &)>;

    using Symbols = QVector<SymSeek::Symbol>;

    struct SymbolsInBinary
    {
        QString binaryPath;
        Symbols symbols;
    };

    inline QString toQString(std::string const & string)
    {
        return QString::fromStdString(string);
    }

    inline QString toQString(std::wstring const & string)
    {
        return QString::fromStdWString(string);
    }

    template<typename T = String>
    T toString(QString const & string)
    {
        if constexpr(std::is_same_v<typename T::value_type, char>)
        {
            return string.toStdString();
        }
        else
        {
            return string.toStdWString();
        }
    }

    class SymbolSeeker: public QObject
    {
        Q_OBJECT
    public:
        enum class ProgressStatus
        {
            Start,
            Finish,
            Reject,
        };
        Q_ENUM(ProgressStatus);

    public Q_SLOTS:
        QVector<SymbolsInBinary> findSymbols(
            QString const & directoryPath, QStringList const & masks, SymbolHandler handler = {});

        void interrupt();

    Q_SIGNALS:
        void startProcessingItems(size_t count);
        void itemStatus(QString binaryPath, ProgressStatus status);
        void itemsRemaining(size_t);
        void interrupted();

    private:
        bool m_interruptFlag = false;
    };
}
