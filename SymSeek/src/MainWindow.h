#pragma once

#include <memory>

#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtWidgets/QMainWindow>

#include <symseek/symseek.h>

#include "SymbolsModel.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class AsyncSeeker: public QThread
{
    Q_OBJECT

public:
    AsyncSeeker(QString const & directory, QStringList const & masks,
                SymSeek::SymbolHandler handler = {}, QObject * parent = nullptr);

    SymSeek::SymbolSeeker const * seeker() const;
    SymSeek::SymbolSeeker * seeker();

    QVector<SymSeek::SymbolsInBinary> result() const;

protected:
    void run() override;

private:
    SymSeek::SymbolSeeker m_seeker;
    QVector<SymSeek::SymbolsInBinary> m_result;
    QString m_directory;
    QStringList m_masks;
    SymSeek::SymbolHandler m_handler;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget * parent = nullptr);

    void doSearch();

    ~MainWindow();

private:
    std::unique_ptr<Ui::MainWindow> m_ui;
    SymbolsModel m_model;
};
