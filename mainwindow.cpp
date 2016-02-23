#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "QFileDialog"
#include "QDirIterator"
#include "QStringList"
#include "QDebug"
#include "QPoint"
#include "QListWidget"
#include "QFile"
#include "QThread"
#include "QtCore"
#include "QMessageBox"
#include "QMenuBar"
#include "QTextEdit"
#include "QDesktopServices"
#include <QInputDialog>

#include "aboutdialog.h"
#include "ui_aboutdialog.h"

bool cleared = false;                                       //if the directory list is clear
QString keyword;                                            //keyword to find
int current = 0;                                            //current ammount of fxb files found
int max_size = 10;                                          //max size until array is doubled

QString *list = new QString[max_size];

QString dir;                                                //directory

QString line;                                               //line for input

QFile inputFile;                                            //file dir


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->startButton->setEnabled(false);                     //set start parse disabled
    ui->progressBar->setValue(0);                           //0% on progress bar
    ui->progressBar->setEnabled(false);                     //set progress bar disabled
    ui->actionOpen_File_Location->setEnabled(false);        //disable the file locator menu bar

}




MainWindow::~MainWindow()
{
    delete ui;
    delete [] list;                                         //make sure list is deleted
}


void MainWindow::on_folderButton_clicked()                  //when opening where fxb files are located...
{


    dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                                                 "/home",QFileDialog::DontResolveSymlinks);     //this finds and stores directory



    if (dir.size() < 4)                                     //if the directory string is less than "C://" <- 4 chars, gtfo
    {
        ui->fxbLabel->setText("<font color = 'red'>Error: Invalid/No File Selected</font>");
        qDebug() << "Cannot Select Root";
    }
    else
    {

        QString fxbText = "<font color = 'green'>.fxb files found in </font>" + dir;
        ui->fxbLabel->setText(fxbText);                     //refresh text
        ui->startButton->setEnabled(true);                  //enable the start button
        QDirIterator it(dir, QStringList() << "*.fxb", QDir::Files, QDirIterator::Subdirectories); //iterate your files with search for .fxb extension
        while (it.hasNext()) {
            if(current != max_size)                         //if not ready to be doubled
            {
                list[current] = it.next();
                current++;

            }
            else                                            //double
            {
                max_size *= 2;                              //double max size
                QString *temp = new QString[max_size];      //classic dynamic mem
                for(int i = 0; i < current; i++)
                {
                    temp[i] = list[i];
                }
                delete [] list;
                list = temp;
                temp->clear();
                qDebug() << max_size;
            }




        }


    }



    for(int i = 0; i < current; i++)
    {
        ui->listWidget->addItem(list[i]);                   //put all items into list
    }




}




void MainWindow::on_startButton_clicked()                   //pressed start button (makes everything slow)
{
    if(cleared != true || keyword == "")
    {
        QMessageBox::critical(this, tr("fxbViewer"), tr("Input keyword"));
    }
    else
    {
    for(int i = 0; i < current; i++)
    {
        QFile inputFile(list[i]);
        if (inputFile.open(QIODevice::ReadOnly))
        {
           ui->progressBar->setEnabled(true);
           ui->progressBar->setMaximum(current);
           ui->progressBar->setValue(i);


           QTextStream in(&inputFile);
           while (!in.atEnd())
           {
                line = in.readLine();
                line = line.toLower();
                if(line.contains(keyword))
                {
                    ui->keywordList->addItem(list[i]);
                }
           }
           inputFile.close();
        }
    }
    ui->progressBar->setValue(0);
    ui->progressBar->setEnabled(false);
    }

}

void MainWindow::on_lineEdit_cursorPositionChanged(int arg1, int arg2)
{
    if(cleared == false)
    {
        ui->lineEdit->setText("");
        cleared = true;
    }
    keyword = ui->lineEdit->text();
    keyword = keyword.toLower();
    qDebug() << keyword;


}

void MainWindow::on_pushButton_clicked()
{
    ui->keywordList->clear();

}

void MainWindow::on_clearSButton_clicked()
{

    ui->fxbLabel->setText(".fxb files in folder");
    ui->listWidget->clear();
    list->clear();
    for(int i = 0; i < max_size; i++)
    {
        line[i]=NULL;
    }
    current = 0;
    ui->keywordList->clear();
    ui->startButton->setEnabled(false);
    dir.clear();
    qDebug() << "CLEAR SEARCH";
}


//right click menu maker
void MainWindow::ShowContextMenu(const QPoint& pos) // this is a slot
{
    // for most widgets
    QPoint globalPos = ui->listWidget->mapToGlobal(pos);

    // for QAbstractScrollArea and derived classes you would use:
    // QPoint globalPos = myWidget->viewport()->mapToGlobal(pos);

    QMenu myMenu;
    myMenu.addAction("Menu Item 1");
    // ...

    QAction* selectedItem = myMenu.exec(globalPos);
    if (selectedItem)
    {
        // something was chosen, do stuff
    }
    else
    {
        // nothing was chosen
    }
}


void MainWindow::on_actionAbout_triggered()
{
    AboutDialog mDialog;
    mDialog.setModal(true);
    mDialog.exec();
}


void MainWindow::on_actionOpen_triggered()
{
    MainWindow::on_folderButton_clicked();
}


void MainWindow::on_actionOpen_File_Location_triggered()
{
    //QString curdir = NULL;
    //curdir = ui->listWidget->currentItem()->text();
    QMessageBox::critical(this, tr("fxbViewer"), tr("Coming Soon"));
}
