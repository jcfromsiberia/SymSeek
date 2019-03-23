#pragma once

#include <functional>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QPair>
#include <QtCore/QVector>

namespace SymSeek
{
    enum class Access
    {
        Public,
        Protected,
        Private
    };

    enum class NameType
    {
        Function,
        Method,
        Variable
    };

    enum class Convention
    {
        Cdecl,
        Stdcall,
        Pascal,
        Fastcall,    //For MSVC only
        Vectorcall,  //For MSVC only
    };

    // TODO Squeeze this struct
    struct Symbol
    {
        NameType type = NameType::Function;
        Access access = Access::Public;  // When type == Method
        Convention convention = Convention::Cdecl;
        enum Modifiers
        {
            None       = 0b0000,
            IsConst    = 0b0001,  // When type == Method or Variable
            IsVolatile = 0b0010,  // When type == Variable
            IsVirtual  = 0b0100,  // When type == Method
            IsStatic   = 0b1000,  // When type == Method
        };  //:3 bit field?
        int modifiers = None;
        bool implements = true;   // Implements or Imports?
        QString mangledName;
        QString demangledName;
    };

    enum class SymbolHandlerAction
    {
        // Add the symbol to the result
        Add,

        // Skip the symbol,
        Skip,

        // Stop processing the binary
        Stop
    };

    using SymbolHandler = std::function<SymbolHandlerAction(Symbol const &)>;

    using Symbols = QVector<Symbol>;

    struct SymbolsInBinary
    {
        QString binaryPath;
        Symbols symbols;
    };

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
        QVector<SymbolsInBinary> findSymbols(QString const & directoryPath, QStringList const & masks,
                SymbolHandler handler = {});

        void interrupt();

    Q_SIGNALS:
        void startProcessingItems(size_t count);
        void itemStatus(QString binaryPath, SymSeek::SymbolSeeker::ProgressStatus status);
        void itemsRemaining(size_t);
        void interrupted();

    private:
        bool m_interruptFlag = false;
    };
}
