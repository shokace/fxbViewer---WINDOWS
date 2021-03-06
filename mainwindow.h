#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCore>
#include <QThread>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_folderButton_clicked();

    void on_startButton_clicked();

    void on_lineEdit_cursorPositionChanged(int arg1, int arg2);

    void on_pushButton_clicked();

    void on_clearSButton_clicked();

    void ShowContextMenu(const QPoint&);

    void on_actionAbout_triggered();

    void on_actionOpen_triggered();

    void on_actionOpen_File_Location_triggered();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
