#include "resultmodel.h"
#include <QFileInfo>

namespace fxb {
void ResultModel::setLibrary(const QVector<LibraryEntry> &entries)
{
    beginResetModel();
    rows_.clear();
    for (const LibraryEntry &entry : entries) {
        if (!entry.bank.isValid()) continue;
        const auto &bank = entry.bank;
        auto add = [&](const QString &label, const QString &type, const QString &text) {
            Row row;
            row.name = label;
            row.bank = QFileInfo(entry.path).fileName();
            row.type = type;
            row.plugin = bank.pluginId;
            row.path = entry.path;
            row.search = label + '\n' + row.bank + '\n' + row.path + '\n' + row.plugin + '\n' + text;
            row.detail = entry.path + QStringLiteral("\nFormat: %1   Plugin ID: %2\n").arg(bank.format, bank.pluginId)
                + bank.warnings.join('\n');
            if (!text.isEmpty()) row.detail += QStringLiteral("\nEmbedded text (may include non-preset metadata):\n") + text;
            rows_.push_back(std::move(row));
        };
        if (bank.presets.isEmpty()) {
            const bool chunk = bank.format == QStringLiteral("FBCh");
            add(chunk ? QStringLiteral("Embedded text — names unavailable") : QStringLiteral("Empty bank"),
                chunk ? QStringLiteral("Bank text") : QStringLiteral("Empty bank"), bank.embeddedText);
        } else for (const Preset &preset : bank.presets) {
            const QString name = preset.name.isEmpty() ? QStringLiteral("Unnamed preset") : preset.name;
            add(QStringLiteral("%1. %2").arg(preset.number).arg(name),
                bank.format == QStringLiteral("FPCh") ? QStringLiteral("Preset + text") : QStringLiteral("Preset"),
                bank.embeddedText);
        }
    }
    endResetModel();
}

int ResultModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : rows_.size(); }
int ResultModel::columnCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : 5; }

QVariant ResultModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size() || index.column() < 0 || index.column() >= 5) return {};
    const Row &row = rows_[index.row()];
    if (role == SearchRole) return row.search;
    if (role == PathRole || role == Qt::ToolTipRole) return row.path;
    if (role == DetailRole) return row.detail;
    if (role != Qt::DisplayRole) return {};
    switch (index.column()) {
    case 0: return row.name;
    case 1: return row.bank;
    case 2: return row.type;
    case 3: return row.plugin;
    default: return row.path;
    }
}

QVariant ResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    const QStringList headers{QStringLiteral("Preset / match source"), QStringLiteral("Bank / file"),
                              QStringLiteral("Result type"), QStringLiteral("Plugin ID"), QStringLiteral("Location")};
    return section >= 0 && section < headers.size() ? headers[section] : QVariant();
}

SearchModel::SearchModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void SearchModel::setQuery(const QString &query)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    query_ = query.trimmed();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    query_ = query.trimmed();
    invalidateFilter();
#endif
}

bool SearchModel::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    return query_.isEmpty() || sourceModel()->index(row, 0, parent).data(ResultModel::SearchRole)
        .toString().contains(query_, Qt::CaseInsensitive);
}
}
