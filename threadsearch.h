#ifndef THREADSEARCH_H
#define THREADSEARCH_H
#include <QtCore>

class threadsearch : public QThread
{
public:
    threadsearch();
    void run(QString keyword, bool cleared, int current, QString *list, QString line);
};

#endif // THREADSEARCH_H
