#pragma once

#include "bankreader.h"
#include <QThread>
#include <QStringList>
#include <atomic>
#include <functional>

namespace fxb {
struct LibraryEntry {
    QString path;
    BankDocument bank;
};
struct ScanResult {
    QVector<LibraryEntry> entries;
    QStringList issues;
    int discovered = 0;
    bool cancelled = false;
};
struct ScanLimits {
    int files = 10000;
    int presets = 100000;
    qint64 textCharacters = 64 * 1024 * 1024;
};

QString canonicalFolder(const QString &path);
ScanResult scanFolders(const QStringList &folders, const std::atomic_bool &cancelled,
                       const std::function<void(int, int)> &progress = {},
                       const ScanLimits &limits = {});

class ScanWorker final : public QThread {
    Q_OBJECT
public:
    explicit ScanWorker(QStringList folders, QObject *parent = nullptr);
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
};
}
