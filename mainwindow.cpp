#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "QFileDialog"
#include "QFile"
#include "QTextStream"
#include "QString"
#include "QList"
#include "QTextStream"
#include "QLineEdit"
#include "QDebug"
#include "dialog.h"

bool cleared = false;



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

/****************File Button****************/
/*  when clicking the file search button,
    qfiledialog will open the file dialog
    and will try to open fxb files*/
/****************File Button****************/
void MainWindow::on_selectFilesButton_clicked()
{

    Dialog dialog;
    dialog.setModal(true);
    dialog.exec();


}
/****************Parse Button****************/
/*  when clicking the parse button,
    the file selected will be parsed
/****************File Button****************/
void MainWindow::on_parseButton_clicked()
{
    ui->parseButton->setEnabled(false);
    ui->stopParseButton->setEnabled(true);
}


/****************Parse Button****************/
/*  when clicking the stop parse button,
    the file selected will stop being parsed
/****************File Button****************/
void MainWindow::on_stopParseButton_clicked()
{
    ui->parseButton->setEnabled(true);
    ui->stopParseButton->setEnabled(false);
}

/*****************Clear All*****************/
/*      EVERYTHING WILL BE DESTROYED
/****************File Button****************/
void MainWindow::on_clearAllButton_clicked()
{

}

/* Just clears the Keyword Edit area for user. Users love dat shit*/
void MainWindow::on_KeywordEdit_cursorPositionChanged(int arg1, int arg2)
{

    if(cleared == false)
    {
       ui->KeywordEdit->clear();
        cleared = true;
    }


}



