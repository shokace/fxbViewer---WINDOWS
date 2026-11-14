#pragma once
#include "library.h"
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

namespace fxb {
class ResultModel final : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Roles { SearchRole = Qt::UserRole, PathRole, DetailRole };
    explicit ResultModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}
    void setLibrary(const QVector<LibraryEntry> &entries);
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
private:
    struct Row { QString name, bank, type, plugin, path, search, detail; };
    QVector<Row> rows_;
};

class SearchModel final : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit SearchModel(QObject *parent = nullptr);
    void setQuery(const QString &query);
protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
private:
    QString query_;
};
}
