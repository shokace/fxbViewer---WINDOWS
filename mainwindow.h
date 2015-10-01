#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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
    void on_selectFilesButton_clicked();

    void on_parseButton_clicked();

    void on_stopParseButton_clicked();

    void on_clearAllButton_clicked();

    void on_KeywordEdit_cursorPositionChanged(int arg1, int arg2);

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
