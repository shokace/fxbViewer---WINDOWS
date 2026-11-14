#pragma once

#include <QMainWindow>
#include <memory>
#include "core/library.h"

class QSettings;
class QCheckBox;
class QComboBox;
class QListWidget;
class QLineEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QTableView;
class QPlainTextEdit;
class QAction;
namespace fxb { class ResultModel; class SearchModel; }

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &settingsPath = {}, QWidget *parent = nullptr, bool demoMode = false);
    ~MainWindow() override;
    bool addFolder(const QString &path);
    QStringList folders() const { return folders_; }
    bool isScanning() const { return worker_ != nullptr; }
public slots:
    void refreshLibrary();
    void rebuildLibrary();
    void cancelScan();
    void clearLibrary();
    void removeSelectedFolder();
    void revealSelectedFile();
    void copySelectedPath();
signals:
    void libraryReady();
protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
private:
    void chooseFolder();
    void startScan(bool forceRead);
    void updateSearch();
    void saveFolders();
    void updateControls();
    void updateCount();
    void showSelection();
    QString selectedPath() const;

    std::unique_ptr<QSettings> settings_;
    QStringList folders_;
    fxb::ScanWorker *worker_ = nullptr;
    bool discardScan_ = false;
    int indexedFiles_ = 0;
    QStringList issues_;
    fxb::ResultModel *model_;
    fxb::SearchModel *searchModel_;
    QListWidget *folderList_;
    QLineEdit *search_;
    QTableView *table_;
    QPlainTextEdit *details_;
    QLabel *status_;
    QLabel *count_;
    QLabel *emptyState_;
    QCheckBox *namesOnly_;
    QCheckBox *typos_;
    QComboBox *sortChoice_;
    bool demoMode_ = false;
    QProgressBar *progress_;
    QPushButton *addButton_;
    QPushButton *removeButton_;
    QPushButton *refreshButton_;
    QPushButton *cancelButton_;
    QPushButton *revealButton_;
    QPushButton *copyButton_;
    QPushButton *issuesButton_;
    QAction *addAction_;
    QAction *refreshAction_;
    QAction *rebuildAction_;
};
