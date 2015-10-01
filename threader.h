#ifndef THREADER
#define THREADER

#include <QMainWindow>
#include <QtCore>

class Thread : public QThread
{
public:
    Thread();
    void run();
    QString name;
};

#endif // THREADER

