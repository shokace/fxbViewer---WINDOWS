#include "library.h"
#include "searchindex.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <algorithm>

namespace fxb {
namespace {
FileStamp stampFor(const QFileInfo &info) {
    return {info.size(), info.lastModified().toMSecsSinceEpoch(), info.metadataChangeTime().toMSecsSinceEpoch()};
}
}
QString canonicalFolder(const QString &path)
{
    const QFileInfo info(path);
    return info.isDir() ? info.canonicalFilePath() : QString();
}

ScanResult scanFolders(const QStringList &folders, const std::atomic_bool &cancelled,
                       const std::function<void(int, int)> &progress, const ScanLimits &limits,
                       const ScanCache &cache, bool forceRead)
{
    QElapsedTimer timer; timer.start();
    ScanResult result;
    QSet<QString> files;
    QSet<QString> visitedFolders;
    if (limits.files < 1 || limits.presets < 0 || limits.textCharacters < 0) {
        result.issues += QStringLiteral("Invalid indexing limits.");
        return result;
    }
    bool fileLimitReached = false;
    for (const QString &folder : folders) {
        if (cancelled.load()) { result.cancelled = true; return result; }
        const QString canonical = canonicalFolder(folder);
        if (canonical.isEmpty() || !QFileInfo(canonical).isReadable()) {
            result.issues += QStringLiteral("Folder unavailable: %1").arg(folder);
            continue;
        }
        if (visitedFolders.contains(canonical)) continue;
        visitedFolders.insert(canonical);
        // Do not follow directory symlinks: prevents cycles and unbounded walks.
        QDirIterator it(canonical, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (cancelled.load()) { result.cancelled = true; return result; }
            it.next();
            const QFileInfo info = it.fileInfo();
            if (info.isDir()) {
                if (!info.isReadable()) result.issues += QStringLiteral("Folder unreadable: %1").arg(info.filePath());
                continue;
            }
            const QString suffix = info.suffix().toLower();
            if (suffix != QStringLiteral("fxb") && suffix != QStringLiteral("fxp")) continue;
            const QString path = info.canonicalFilePath();
            if (path.isEmpty()) { result.issues += QStringLiteral("File unavailable: %1").arg(info.filePath()); continue; }
            if (files.contains(path)) continue;
            if (files.size() >= limits.files) { fileLimitReached = true; break; }
            files.insert(path);
            if (progress && files.size() % 64 == 0) progress(0, 0);
        }
        if (fileLimitReached) {
            result.issues += QStringLiteral("%1-file limit reached. Choose smaller folders to index the rest.").arg(limits.files);
            break;
        }
    }
    QStringList paths = files.values();
    std::sort(paths.begin(), paths.end());
    result.discovered = paths.size();
    qint64 textBudget = limits.textCharacters;
    int presetBudget = limits.presets;
    for (int i = 0; i < paths.size(); ++i) {
        if (cancelled.load()) { result.cancelled = true; return result; }
        QFileInfo info(paths[i]);
        const FileStamp stamp = stampFor(info);
        const auto cached = cache.constFind(paths[i]);
        BankDocument bank;
        if (!forceRead && info.isReadable() && cached != cache.constEnd()
            && stamp.bytes >= 0 && stamp == cached->stamp && cached->bank.isValid()) {
            bank = cached->bank;
            ++result.cachedFiles;
        } else {
            bank = BankReader::readFile(paths[i]);
            ++result.filesRead;
            info.refresh();
            if (bank.isValid() && !(stampFor(info) == stamp))
                bank.error = QStringLiteral("File changed while being indexed. Refresh to retry.");
        }
        if (!bank.isValid()) result.issues += QStringLiteral("%1: %2").arg(paths[i], bank.error);
        else {
            const qint64 characters = bank.embeddedText.size();
            if (characters > textBudget || bank.presets.size() > presetBudget) {
                result.issues += QStringLiteral("Library memory limit reached. Choose smaller folders to index the rest.");
                break;
            }
            textBudget -= characters;
            presetBudget -= bank.presets.size();
            result.entries.push_back({paths[i], std::move(bank), stamp});
        }
        if (progress && (i % 16 == 0 || i + 1 == paths.size())) progress(i + 1, paths.size());
    }
    result.cancelled = cancelled.load();
    result.elapsedMs = timer.elapsed();
    return result;
}

ScanWorker::ScanWorker(QStringList folders, QObject *parent, std::shared_ptr<const SearchIndex> previous, bool forceRead)
    : QThread(parent), folders_(std::move(folders)), previous_(std::move(previous)), forceRead_(forceRead) {}

void ScanWorker::run()
{
    QElapsedTimer timer; timer.start();
    ScanCache cache;
    if (previous_) for (const auto &entry : previous_->entries()) cache.insert(entry.path, entry);
    result_ = scanFolders(folders_, cancelled_, [this](int completed, int total) {
        emit progress(completed, total);
    }, {}, cache, forceRead_);
    if (!result_.cancelled) {
        bool unchanged = previous_ && result_.filesRead == 0 && previous_->bankCount() == result_.entries.size();
        if (unchanged) for (int i = 0; i < result_.entries.size(); ++i)
            if (result_.entries[i].path != previous_->entries()[i].path) { unchanged = false; break; }
        if (unchanged) result_.index = previous_;
        else {
            emit progress(0, 0);
            result_.index = SearchIndex::build(result_.entries, &cancelled_);
        }
    }
    result_.cancelled = result_.cancelled || cancelled_.load();
    result_.elapsedMs = timer.elapsed();
}
}
