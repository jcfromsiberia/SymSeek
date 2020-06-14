#include "SymbolsModel.h"

#include <QtCore/QDir>

#include <QtGui/QColor>

using namespace SymSeek::QtUI;

SymbolsModel::SymbolsModel(QObject *parent)
: QAbstractTableModel(parent)
{
}

int SymbolsModel::rowCount(QModelIndex const & parent) const
{
    return m_binariesToSymbols.size();
}

int SymbolsModel::columnCount(QModelIndex const & parent) const
{
    return 5;
}

QVariant SymbolsModel::data(QModelIndex const & index, int role) const
{
    int const row = index.row();
    int const col = index.column();

    Q_ASSERT(row < m_binariesToSymbols.size());

    using namespace SymSeek;

    auto const & [binRef, sym] = m_binariesToSymbols[row];

    switch (col)
    {
        case 0:
            {
                if (role == Qt::ToolTipRole)
                {
                    return *binRef.string();
                }
                else if (role == Qt::DisplayRole)
                {
                    QString path = *binRef.string();
                    int index = binRef.lastIndexOf('/');
                    if(index > -1)
                        return path.right(path.size() - index - 1);
                    return path;
                }
            }
            break;
        case 1:
        {
            if (role == Qt::DisplayRole)
            {
                return sym.raw.implements ? "EXP" : "IMP";  // TODO Replace with fancy icons!
            }
        }
            break;
        case 2:
            {
                if (role == Qt::DisplayRole)
                {
                    return sym.demangledName ? "C++" : "C";
                }
            }
            break;
        case 3:
            {
                if (role == Qt::DecorationRole) {
                    switch (sym.access)
                    {
                        case Access::Public:
                            return QColor{ "limegreen" };  // TODO Replace with fancy icons!
                        case Access::Protected:
                            return QColor{ "gold" };
                        case Access::Private:
                            return QColor{ "firebrick" };
                    };
                }
                if (role == Qt::DisplayRole)
                {
                    QString text;
                    if(sym.modifiers & Symbol::IsStatic)
                        text += "static ";
                    if(sym.modifiers & Symbol::IsVirtual)
                        text += "virtual ";
                    if(sym.modifiers & Symbol::IsConst)
                        text += "const ";
                    if(sym.modifiers & Symbol::IsVolatile)
                        text += "volatile ";
                    switch(sym.type)
                    {
                        case NameType::Function:
                            text += "function";
                            break;
                        case NameType::Method:
                            text += "method";
                            break;
                        case NameType::Variable:
                            text += "variable";
                    }
                    return text;
                }
            }
            break;
        case 4:
            {
                if (role == Qt::DisplayRole)
                {
                    return toQString(sym.demangledName ? sym.demangledName.value() : sym.raw.name);
                }

                if (role == Qt::ToolTipRole)
                {
                    return toQString(sym.raw.name);
                }
            }
            break;
        default:;
    };

    return {};
}

void SymbolsModel::setSymbols(QVector<SymbolsInBinary> symbolsInBinaries)
{
    Q_EMIT beginResetModel();
    m_binaries.clear();
    m_binariesToSymbols.clear();
    m_binaries.reserve(symbolsInBinaries.size());
    for (auto const & symsInBin: symbolsInBinaries)
    {
        m_binaries.push_back(symsInBin.binaryPath);
        QStringRef binRef{ &m_binaries.last() };
        for(auto const & symbol: symsInBin.symbols)
        {
            m_binariesToSymbols.push_back({ binRef, symbol });
        }
    }
    Q_EMIT endResetModel();
}

QVariant SymbolsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if (role == Qt::DisplayRole)
        {
            static QStringList const labels = {"IMG", "DIR", "LNG", "EXT", "SYMBOL"};
            Q_ASSERT(section < labels.size());
            return labels[section];
        }
        else if (role == Qt::ToolTipRole)
        {
            static QStringList const labels = {"Image", "Directory(Import/Export)",
                                               "Language", "Extra modifiers", "Symbol name"};
            Q_ASSERT(section < labels.size());
            return labels[section];
        }
    }
    return QAbstractItemModel::headerData(section, orientation, role);
}
