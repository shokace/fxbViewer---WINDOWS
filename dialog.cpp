#include "dialog.h"
#include "ui_dialog.h"
#include "QFileDialog"
#include "QDebug"



Dialog::Dialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);

    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    qDebug() << dir;

}







Dialog::~Dialog()
{
    delete ui;
}
