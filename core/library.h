#pragma once

#include "bankreader.h"
#include <QThread>
#include <QStringList>
#include <QHash>
#include <atomic>
#include <functional>
#include <memory>

namespace fxb {
class SearchIndex;
struct FileStamp {
    qint64 bytes = -1;
    qint64 modified = -1;
    qint64 changed = -1;
    bool operator==(const FileStamp &other) const {
        return bytes == other.bytes && modified == other.modified && changed == other.changed;
    }
};
struct LibraryEntry {
    QString path;
    BankDocument bank;
    FileStamp stamp{};
};
using ScanCache = QHash<QString, LibraryEntry>;
struct ScanResult {
    QVector<LibraryEntry> entries;
    QStringList issues;
    int discovered = 0;
    bool cancelled = false;
    int filesRead = 0;
    int cachedFiles = 0;
    qint64 elapsedMs = 0;
    std::shared_ptr<const SearchIndex> index;
};
struct ScanLimits {
    int files = 10000;
    int presets = 100000;
    qint64 textCharacters = 64 * 1024 * 1024;
};

QString canonicalFolder(const QString &path);
ScanResult scanFolders(const QStringList &folders, const std::atomic_bool &cancelled,
                       const std::function<void(int, int)> &progress = {},
                       const ScanLimits &limits = {}, const ScanCache &cache = {}, bool forceRead = false);

class ScanWorker final : public QThread {
    Q_OBJECT
public:
    explicit ScanWorker(QStringList folders, QObject *parent = nullptr,
                        std::shared_ptr<const SearchIndex> previous = {}, bool forceRead = false);
    void cancel() { cancelled_.store(true); }
    const ScanResult &result() const { return result_; } // Read only after finished.
signals:
    void progress(int completed, int total);
protected:
    void run() override;
private:
    QStringList folders_;
    std::atomic_bool cancelled_{false};
    ScanResult result_;
    std::shared_ptr<const SearchIndex> previous_;
    bool forceRead_ = false;
};
}
