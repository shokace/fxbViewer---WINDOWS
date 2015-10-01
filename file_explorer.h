#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include <QDialog>
#include <QtCore>
#include <QtGui>
#include <QFileSystemModel>


namespace Ui {
class file_explorer;
}

class file_explorer : public QDialog
{
    Q_OBJECT

public:
    explicit file_explorer(QWidget *parent = 0);
    ~file_explorer();

private slots:

    void on_treeView_clicked(QModelIndex index);

private:
    Ui::file_explorer *ui;
    QFileSystemModel *dirmodel;
    QFileSystemModel *filemodel;
};

#endif // FILE_EXPLORER_H
