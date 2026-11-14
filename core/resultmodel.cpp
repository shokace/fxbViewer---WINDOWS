#include "resultmodel.h"
#include <QFileInfo>
#include <QFont>
#include <QColor>

namespace fxb {
void ResultModel::setLibrary(const QVector<LibraryEntry> &entries) { setIndex(SearchIndex::build(entries)); }
void ResultModel::setIndex(std::shared_ptr<const SearchIndex> index) {
    beginResetModel(); index_ = std::move(index); endResetModel();
}
int ResultModel::rowCount(const QModelIndex &parent) const { return parent.isValid() || !index_ ? 0 : index_->size(); }
int ResultModel::columnCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : 5; }
QVariant ResultModel::data(const QModelIndex &idx, int role) const {
    if (!idx.isValid() || !index_ || idx.row() < 0 || idx.row() >= index_->size() || idx.column() < 0 || idx.column() >= 5) return {};
    const auto &record = index_->record(idx.row());
    const auto &entry = index_->entry(idx.row());
    if (role == PathRole || role == Qt::ToolTipRole) return entry.path;
    if (role == Qt::FontRole && idx.column() == 0) { QFont font; font.setWeight(QFont::DemiBold); return font; }
    if (role == SearchRole) return record.name + '\n' + entry.path + '\n' + entry.bank.pluginId + '\n' + entry.bank.embeddedText;
    if (role == DetailRole) {
        QString detail = entry.path + QStringLiteral("\nFormat: %1   Plugin ID: %2\n").arg(entry.bank.format, entry.bank.pluginId)
            + entry.bank.warnings.join('\n');
        if (!entry.bank.embeddedText.isEmpty()) {
            const QString preview = entry.bank.embeddedText.left(4096).section('\n', 0, 79);
            detail += QStringLiteral("\nEmbedded text preview (may include non-preset metadata):\n") + preview;
            if (preview.size() < entry.bank.embeddedText.size()) detail += QStringLiteral("\n… Preview limited; all indexed text remains searchable.");
        }
        return detail;
    }
    if (role != Qt::DisplayRole) return {};
    switch (idx.column()) {
    case 0:
        if (record.number == 0) return entry.bank.format == "FBCh" ? QStringLiteral("Bank text · names unavailable") : QStringLiteral("Empty bank");
        return QStringLiteral("%1. %2").arg(record.number).arg(record.name.isEmpty() ? QStringLiteral("Unnamed preset") : record.name);
    case 1: return QFileInfo(entry.path).fileName();
    case 2: return record.number == 0 ? (entry.bank.format == "FBCh" ? QStringLiteral("Bank text") : QStringLiteral("Empty bank")) : QStringLiteral("Preset");
    case 3: return entry.bank.pluginId;
    default: return entry.path;
    }
}
QVariant ResultModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    const QStringList headers{QStringLiteral("Preset / match"), QStringLiteral("Bank"),
                              QStringLiteral("Match source"), QStringLiteral("Plugin"), QStringLiteral("Location")};
    return section >= 0 && section < headers.size() ? headers[section] : QVariant();
}
SearchModel::SearchModel(QObject *parent) : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true); setSortCaseSensitivity(Qt::CaseInsensitive); sort(0);
}
void SearchModel::setSourceModel(QAbstractItemModel *model) {
    disconnect(resetConnection_);
    QSortFilterProxyModel::setSourceModel(model);
    if (model) resetConnection_ = connect(model, &QAbstractItemModel::modelReset, this, &SearchModel::rebuild);
    rebuild();
}
void SearchModel::setQuery(const QString &query) {
    const QString trimmed = query.trimmed();
    if (query_ == trimmed) return;
    query_ = trimmed; rebuild();
}
void SearchModel::setOptions(SearchOptions options) {
    if (options_.namesOnly == options.namesOnly && options_.allowTypos == options.allowTypos) return;
    options_ = options; rebuild();
}
void SearchModel::setRelevanceSort(bool enabled) { relevance_ = enabled; invalidate(); }
void SearchModel::rebuild() {
    const auto *model = qobject_cast<const ResultModel *>(sourceModel());
    results_ = {};
    positions_.clear();
    if (model && model->searchIndex()) {
        results_ = model->searchIndex()->search(query_, options_);
        positions_.fill(-1, model->rowCount());
        for (int i = 0; i < results_.hits.size(); ++i) positions_[results_.hits[i].row] = i;
    }
    invalidate();
}
bool SearchModel::filterAcceptsRow(int row, const QModelIndex &) const { return row >= 0 && row < positions_.size() && positions_[row] >= 0; }
bool SearchModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    if (!relevance_ && left.column() != 0) return QSortFilterProxyModel::lessThan(left, right);
    const int l = positions_.value(left.row(), -1), r = positions_.value(right.row(), -1);
    if (l < 0 || r < 0) return left.row() < right.row();
    const int a = results_.hits[l].score, b = results_.hits[r].score;
    if (relevance_ && a != b) return a > b;
    const auto index = qobject_cast<const ResultModel *>(sourceModel())->searchIndex();
    const int compared = QString::compare(index->record(left.row()).normalizedName, index->record(right.row()).normalizedName);
    if (compared) return compared < 0;
    const int path = QString::compare(index->entry(left.row()).path, index->entry(right.row()).path);
    return path ? path < 0 : left.row() < right.row();
}
QVariant SearchModel::data(const QModelIndex &idx, int role) const {
    if (!idx.isValid()) return {};
    const auto source = mapToSource(idx);
    const int pos = positions_.value(source.row(), -1);
    if (pos >= 0 && !query_.isEmpty()) {
        const auto &hit = results_.hits[pos];
        if (role == Qt::ForegroundRole && idx.column() == 2)
            return QColor(hit.source == MatchSource::SimilarName ? "#f4d391" : "#9fe0cd");
        if ((role == Qt::DisplayRole && idx.column() == 2) || role == ResultModel::MatchRole) return matchSourceLabel(hit.source);
        if (role == ResultModel::ScoreRole) return hit.score;
        if (role == ResultModel::SnippetRole || role == ResultModel::DetailRole) {
            const auto index = qobject_cast<const ResultModel *>(sourceModel())->searchIndex();
            const QString snippet = index->snippet(hit);
            if (role == ResultModel::SnippetRole) return snippet;
            return matchSourceLabel(hit.source) + QStringLiteral("\n") + snippet + QStringLiteral("\n\n")
                + QSortFilterProxyModel::data(idx, role).toString();
        }
    }
    return QSortFilterProxyModel::data(idx, role);
}
}
