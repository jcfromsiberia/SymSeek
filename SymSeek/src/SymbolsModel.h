#pragma once

#include <QtCore/QAbstractTableModel>
#include <QtCore/QVector>
#include <QtCore/QStringList>

#include <symseek/symseek.h>

class SymbolsModel: public QAbstractTableModel
{
    Q_OBJECT

public:
    SymbolsModel(QObject * parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void setSymbols(QVector<SymSeek::SymbolsInBinary> symbols);

private:
    // Preventing extra memory consumption
    struct BinaryToSymbol
    {
        QStringRef binaryRef{};
        SymSeek::Symbol symbol;
    };
    QVector<BinaryToSymbol> m_binariesToSymbols;
    QStringList m_binaries;
};
