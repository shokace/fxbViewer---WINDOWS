#include "file_explorer.h"
#include "ui_file_explorer.h"

file_explorer::file_explorer(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::file_explorer)

{
    ui->setupUi(this);

    QStringList nameFilter;
    nameFilter << "*.fxp" << "*.fxb";

    QString sPath = "C:/";
    dirmodel = new QFileSystemModel(this);
    dirmodel->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);
    dirmodel->setRootPath(sPath);
    ui->treeView->setModel(dirmodel);


    filemodel = new QFileSystemModel(this);
    filemodel->setFilter(QDir::NoDotAndDotDot | QDir::Files);

    filemodel->setRootPath(sPath);
    ui->listView->setModel(filemodel);
}




void file_explorer::on_treeView_clicked(QModelIndex index)
{
    QString sPath = dirmodel->fileInfo(index).absoluteFilePath();
    ui->listView->setRootIndex(filemodel->setRootPath(sPath));
}

file_explorer::~file_explorer()
{
    delete ui;
}
