#pragma once
#include "searchindex.h"
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

namespace fxb {
class ResultModel final : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Roles { SearchRole = Qt::UserRole, PathRole, DetailRole, ScoreRole, SnippetRole, MatchRole };
    explicit ResultModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}
    void setLibrary(const QVector<LibraryEntry> &entries);
    void setIndex(std::shared_ptr<const SearchIndex> index);
    std::shared_ptr<const SearchIndex> searchIndex() const { return index_; }
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
private:
    std::shared_ptr<const SearchIndex> index_;
};

class SearchModel final : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit SearchModel(QObject *parent = nullptr);
    void setSourceModel(QAbstractItemModel *model) override;
    void setQuery(const QString &query);
    void setOptions(SearchOptions options);
    void setRelevanceSort(bool enabled);
    SearchStats stats() const { return results_.stats; }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
private:
    void rebuild();
    QString query_;
    SearchOptions options_;
    SearchResults results_;
    QVector<int> positions_;
    QMetaObject::Connection resetConnection_;
    bool relevance_ = true;
};
}
