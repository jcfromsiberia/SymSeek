#pragma once

import <memory>;

#include <QtCore/QPointer>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QThread>
#include <QtGui/QValidator>
#include <QtWidgets/QMainWindow>

#include "SymbolsModel.h"

import symseek;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class Workspace;
}
QT_END_NAMESPACE

namespace SymSeek::QtUI
{
    class AsyncSeeker: public QThread
    {
        Q_OBJECT

    public:
        AsyncSeeker(QString const & directory, QStringList const & masks,
            SymbolHandler handler = {}, QObject * parent = nullptr);

        SymbolSeeker const * seeker() const;
        SymbolSeeker * seeker();

        QVector<SymbolsInBinary> result() const;

    protected:
        void run() override;

    private:
        SymbolSeeker m_seeker;
        QVector<SymbolsInBinary> m_result;
        QString m_directory;
        QStringList m_masks;
        SymbolHandler m_handler;
    };

    class Workspace : public QWidget
    {
        Q_OBJECT

    public:
        Workspace(QWidget * parent = nullptr);

        ~Workspace();

        void loadSettings(uint index);
        void storeSettings(uint index) const;

    Q_SIGNALS:
        void titleChanged(QString newTitle);

    private:
        void doSearch();

    private:
        std::unique_ptr<Ui::Workspace> m_ui;
        SymbolsModel m_model;
        QSortFilterProxyModel m_proxyModel;

        QPointer<QValidator> m_directoryValidator;
        QPointer<QValidator> m_regexValidator;
    };
}
