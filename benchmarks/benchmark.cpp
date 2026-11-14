#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cstdio>
#include <algorithm>
#include "core/demodata.h"
#include "core/resultmodel.h"
using namespace fxb;

// Retained reference algorithm from 4e34bcf: full combined text, QVariant access,
// case-insensitive substring scan of every row. No ranking or trigram index.
class LinearModel : public QAbstractTableModel {
public:
    QStringList rows;
    explicit LinearModel(const QVector<LibraryEntry> &entries) {
        for (const auto &entry : entries) {
            auto add = [&](const QString &name) { rows += name + '\n' + QFileInfo(entry.path).fileName()
                + '\n' + entry.path + '\n' + entry.bank.pluginId + '\n' + entry.bank.embeddedText; };
            if (entry.bank.presets.isEmpty()) add("Embedded text — names unavailable");
            else for (const auto &p : entry.bank.presets) add(QString("%1. %2").arg(p.number).arg(p.name));
        }
    }
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : rows.size(); }
    int columnCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : 1; }
    QVariant data(const QModelIndex &i, int role) const override {
        return i.isValid() && role == Qt::UserRole ? rows[i.row()] : QVariant();
    }
};
class LinearSearch : public QSortFilterProxyModel {
public:
    QString query;
    void setQuery(const QString &text) { query = text; invalidate(); }
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
        return query.isEmpty() || sourceModel()->index(row, 0, parent).data(Qt::UserRole).toString().contains(query, Qt::CaseInsensitive);
    }
};

template<class Search> double medianQuery(Search &search, const QString &query, int &hits) {
    QElapsedTimer timer;
    QVector<double> times;
    for (int i = 0; i < 15; ++i) {
        search.setQuery(""); search.rowCount();
        timer.start(); search.setQuery(query); hits = search.rowCount();
        times += timer.nsecsElapsed() / 1e6;
    }
    std::sort(times.begin(), times.end()); return times[times.size()/2];
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir folder;
    QString error;
    if (!folder.isValid() || !createDemoLibrary(folder.path(), 256, 64, &error)) { qCritical("%s", qPrintable(error)); return 1; }
    std::atomic_bool cancel{false};
    QElapsedTimer timer; timer.start();
    auto result = scanFolders({folder.path()}, cancel);
    const double scanMs = timer.nsecsElapsed() / 1e6;
    ResultModel model;
    timer.restart(); model.setLibrary(result.entries);
    const double prepareMs = timer.nsecsElapsed() / 1e6;
    SearchModel search; search.setSourceModel(&model); search.rowCount();
    LinearModel linearModel(result.entries);
    LinearSearch linear; linear.setSourceModel(&linearModel); linear.rowCount();
    const QStringList queries{"glass lead", "velvet", "plucked moon", "sub bass", "silver bell", "unfindablexyz", "chorale", "042"};
    QJsonArray timings;
    for (const auto &query : queries) {
        int oldHits = 0, newHits = 0;
        const double before = medianQuery(linear, query, oldHits);
        // Quote the indexed query to compare the SAME substring/phrase semantics.
        const double after = medianQuery(search, '"' + query + '"', newHits);
        if (oldHits != newHits) { qCritical("Result mismatch for %s: %d vs %d", qPrintable(query), oldHits, newHits); return 2; }
        timings += QJsonObject{{"query", query}, {"matches", newHits}, {"linear_median_ms", before},
            {"indexed_median_ms", after}, {"speedup", before / after}, {"candidates_checked", search.stats().candidatesExamined}};
    }
    QVector<double> refreshTimes;
    int reused = 0;
    for (int i = 0; i < 7; ++i) {
        ScanWorker worker({folder.path()}, nullptr, model.searchIndex());
        timer.restart(); worker.start();
        if (!worker.wait(30000) || worker.result().index != model.searchIndex() || worker.result().filesRead != 0) return 3;
        refreshTimes += timer.nsecsElapsed() / 1e6;
        reused = worker.result().cachedFiles;
    }
    std::sort(refreshTimes.begin(), refreshTimes.end());
    QJsonObject report{{"corpus", "256 synthetic parameter banks x 128 presets, plus 64 opaque 128 KiB banks"},
        {"files", result.entries.size()}, {"rows", model.rowCount()}, {"initial_read_ms", scanMs},
        {"initial_search_index_ms", prepareMs}, {"unchanged_refresh_median_ms", refreshTimes[3]},
        {"unchanged_refresh_files_read", 0}, {"unchanged_refresh_files_reused", reused},
        {"iterations_per_query", 15}, {"queries", timings}, {"qt", QT_VERSION_STR},
        {"build", "Release -O2; model/filter/sort timing, not rendering; generated files already in OS cache"}};
    const auto json = QJsonDocument(report).toJson();
    std::fwrite(json.constData(), 1, json.size(), stdout);
}
