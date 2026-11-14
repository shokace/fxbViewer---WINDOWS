#include "mainwindow.h"
#include "core/resultmodel.h"
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QTableView>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(const QString &settingsPath, QWidget *parent) : QMainWindow(parent)
{
    settings_ = settingsPath.isEmpty() ? std::make_unique<QSettings>()
        : std::make_unique<QSettings>(settingsPath, QSettings::IniFormat);
    setWindowTitle(tr("FXB Viewer"));
    setAcceptDrops(true);
    resize(1160, 780);
    setMinimumSize(820, 560);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(14);
    auto *title = new QLabel(tr("FXB Viewer"));
    QFont titleFont = title->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);
    auto *subtitle = new QLabel(tr("Find a remembered preset. Search your sound banks in one place."));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto *toolbar = new QHBoxLayout;
    addButton_ = new QPushButton(tr("Add folder…"));
    addButton_->setObjectName("addFolder");
    refreshButton_ = new QPushButton(tr("Refresh library"));
    refreshButton_->setObjectName("refreshLibrary");
    cancelButton_ = new QPushButton(tr("Cancel indexing"));
    cancelButton_->setObjectName("cancelScan");
    auto *clear = new QPushButton(tr("Clear library"));
    clear->setObjectName("clearLibrary");
    toolbar->addWidget(addButton_);
    toolbar->addWidget(refreshButton_);
    toolbar->addWidget(cancelButton_);
    toolbar->addStretch();
    toolbar->addWidget(clear);
    layout->addLayout(toolbar);

    auto *splitter = new QSplitter;
    auto *sidebar = new QWidget;
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 0, 8, 0);
    sideLayout->addWidget(new QLabel(tr("LIBRARY FOLDERS")));
    folderList_ = new QListWidget;
    folderList_->setObjectName("folderList");
    folderList_->setMinimumWidth(180);
    sideLayout->addWidget(folderList_);
    removeButton_ = new QPushButton(tr("Remove selected folder"));
    removeButton_->setObjectName("removeFolder");
    sideLayout->addWidget(removeButton_);
    auto *hint = new QLabel(tr("Drop folders here to add them. Subfolders are included. Supports .fxb and .fxp files."));
    hint->setWordWrap(true);
    sideLayout->addWidget(hint);
    splitter->addWidget(sidebar);

    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(8, 0, 0, 0);
    search_ = new QLineEdit;
    search_->setObjectName("searchInput");
    search_->setPlaceholderText(tr("Search preset, bank, plugin ID, or embedded text…"));
    search_->setClearButtonEnabled(true);
    search_->setAccessibleName(tr("Search library"));
    contentLayout->addWidget(search_);
    count_ = new QLabel;
    count_->setObjectName("resultCount");
    contentLayout->addWidget(count_);
    model_ = new fxb::ResultModel(this);
    searchModel_ = new fxb::SearchModel(this);
    searchModel_->setSourceModel(model_);
    table_ = new QTableView;
    table_->setObjectName("resultsTable");
    table_->setModel(searchModel_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSortingEnabled(true);
    table_->sortByColumn(1, Qt::AscendingOrder);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(false);
    table_->verticalHeader()->hide();
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setColumnWidth(0, 280);
    table_->setColumnWidth(1, 200);
    table_->setColumnWidth(2, 130);
    table_->setColumnWidth(3, 100);
    contentLayout->addWidget(table_, 3);
    details_ = new QPlainTextEdit;
    details_->setObjectName("resultDetails");
    details_->setReadOnly(true);
    details_->setMaximumBlockCount(1500);
    details_->setPlaceholderText(tr("Select a result to inspect its location and format details."));
    contentLayout->addWidget(details_, 1);
    auto *resultActions = new QHBoxLayout;
    revealButton_ = new QPushButton(tr("Open containing folder"));
    revealButton_->setObjectName("revealFile");
    copyButton_ = new QPushButton(tr("Copy path"));
    copyButton_->setObjectName("copyPath");
    resultActions->addWidget(revealButton_);
    resultActions->addWidget(copyButton_);
    resultActions->addStretch();
    contentLayout->addLayout(resultActions);
    splitter->addWidget(content);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 850});
    layout->addWidget(splitter, 1);

    progress_ = new QProgressBar;
    progress_->setObjectName("scanProgress");
    progress_->hide();
    layout->addWidget(progress_);
    auto *statusLine = new QHBoxLayout;
    status_ = new QLabel(tr("Add a folder to start building your library."));
    status_->setObjectName("statusText");
    status_->setWordWrap(true);
    issuesButton_ = new QPushButton(tr("View issues"));
    issuesButton_->setObjectName("viewIssues");
    statusLine->addWidget(status_, 1);
    statusLine->addWidget(issuesButton_);
    layout->addLayout(statusLine);
    setCentralWidget(central);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    addAction_ = fileMenu->addAction(tr("Add folder…"));
    addAction_->setShortcut(QKeySequence::Open);
    connect(addAction_, &QAction::triggered, this, &MainWindow::chooseFolder);
    refreshAction_ = fileMenu->addAction(tr("Refresh library"));
    refreshAction_->setShortcut(QKeySequence::Refresh);
    connect(refreshAction_, &QAction::triggered, this, &MainWindow::refreshLibrary);
    fileMenu->addAction(tr("Clear library"), this, &MainWindow::clearLibrary);
    fileMenu->addSeparator();
    auto *quitAction = fileMenu->addAction(tr("Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    auto *findAction = editMenu->addAction(tr("Search library"));
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, [this] { search_->setFocus(); search_->selectAll(); });
    auto *copyAction = editMenu->addAction(tr("Copy selected path"));
    copyAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
    connect(copyAction, &QAction::triggered, this, &MainWindow::copySelectedPath);
    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    connect(helpMenu->addAction(tr("How to use")), &QAction::triggered, this, [this] {
        QMessageBox::information(this, tr("Find a preset"), tr("Add folders containing FXB banks or FXP presets. Indexing runs in the background; search filters the completed library as you type. Refresh after changing files.\n\nStandard banks show preset names and positions. Plugin-specific banks expose searchable embedded text where available; those matches are not guaranteed preset names. Compressed or encrypted names cannot be decoded.\n\nSelect a result to copy its path or open its folder. Cancel keeps the previous completed index. Clear library removes folder selections and results, never your files."));
    });
    connect(helpMenu->addAction(tr("About")), &QAction::triggered, this, [this] {
        QMessageBox::about(this, tr("About FXB Viewer"), tr("FXB Viewer 2.0\nSound-bank search and preset discovery.\n\nOriginal project by Shokace / Petar Juric.\nThis application reads preset files; it does not load plugins or play audio."));
    });
    connect(addButton_, &QPushButton::clicked, this, &MainWindow::chooseFolder);
    connect(refreshButton_, &QPushButton::clicked, this, &MainWindow::refreshLibrary);
    connect(cancelButton_, &QPushButton::clicked, this, &MainWindow::cancelScan);
    connect(clear, &QPushButton::clicked, this, &MainWindow::clearLibrary);
    connect(removeButton_, &QPushButton::clicked, this, &MainWindow::removeSelectedFolder);
    connect(folderList_, &QListWidget::currentRowChanged, this, &MainWindow::updateControls);
    connect(search_, &QLineEdit::textChanged, this, [this](const QString &text) {
        searchModel_->setQuery(text);
        updateCount();
        showSelection();
    });
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::showSelection);
    connect(table_, &QTableView::doubleClicked, this, &MainWindow::revealSelectedFile);
    connect(revealButton_, &QPushButton::clicked, this, &MainWindow::revealSelectedFile);
    connect(copyButton_, &QPushButton::clicked, this, &MainWindow::copySelectedPath);
    connect(issuesButton_, &QPushButton::clicked, this, [this] {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Indexing issues"));
        dialog.resize(760, 460);
        QVBoxLayout box(&dialog);
        QPlainTextEdit text;
        text.setReadOnly(true);
        text.setPlainText(issues_.join(QStringLiteral("\n\n")));
        box.addWidget(&text);
        QDialogButtonBox buttons(QDialogButtonBox::Close);
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        box.addWidget(&buttons);
        dialog.exec();
    });

    folders_ = settings_->value("library/folders").toStringList();
    folders_.removeDuplicates();
    folderList_->addItems(folders_);
    updateCount();
    updateControls();
    if (!folders_.isEmpty()) QTimer::singleShot(0, this, &MainWindow::refreshLibrary);
}

MainWindow::~MainWindow()
{
    if (worker_) {
        disconnect(worker_, nullptr, this, nullptr);
        worker_->cancel();
        worker_->wait();
    }
}

void MainWindow::saveFolders()
{
    settings_->setValue("library/folders", folders_);
    settings_->sync();
}

bool MainWindow::addFolder(const QString &path)
{
    if (isScanning()) return false;
    const QString canonical = fxb::canonicalFolder(path);
    if (canonical.isEmpty()) { status_->setText(tr("Folder is unavailable: %1").arg(path)); return false; }
    if (folders_.contains(canonical)) { status_->setText(tr("That folder is already in the library.")); return false; }
    folders_ += canonical;
    auto *item = new QListWidgetItem(canonical, folderList_);
    item->setToolTip(canonical);
    saveFolders();
    refreshLibrary();
    return true;
}

void MainWindow::chooseFolder()
{
    const QString initial = folders_.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation) : folders_.last();
    const QString path = QFileDialog::getExistingDirectory(this, tr("Add preset folder"), initial);
    if (!path.isEmpty()) addFolder(path);
}

void MainWindow::refreshLibrary()
{
    if (isScanning() || folders_.isEmpty()) return;
    discardScan_ = false;
    worker_ = new fxb::ScanWorker(folders_, this);
    status_->setText(tr("Discovering banks and presets…"));
    progress_->setRange(0, 0);
    progress_->show();
    connect(worker_, &fxb::ScanWorker::progress, this, [this](int done, int total) {
        if (discardScan_) return;
        progress_->setRange(0, total);
        progress_->setValue(done);
        if (total) status_->setText(tr("Indexing %1 of %2 files…").arg(done).arg(total));
    });
    connect(worker_, &QThread::finished, this, [this] {
        auto *completed = worker_;
        const auto &result = completed->result();
        if (!discardScan_ && !result.cancelled) {
            model_->setLibrary(result.entries);
            indexedFiles_ = result.entries.size();
            issues_ = result.issues;
            for (const auto &entry : result.entries)
                for (const auto &warning : entry.bank.warnings)
                    issues_ += entry.path + QStringLiteral(": ") + warning;
            status_->setText(result.discovered == 0 ? tr("No .fxb or .fxp files found in these folders.")
                : tr("Indexed %1 of %2 files. %3 notice(s). Refresh to pick up changes.")
                    .arg(indexedFiles_).arg(result.discovered).arg(issues_.size()));
        }
        worker_ = nullptr;
        completed->deleteLater();
        progress_->hide();
        updateCount();
        showSelection();
        updateControls();
        emit libraryReady();
    });
    updateControls();
    worker_->start();
}

void MainWindow::cancelScan()
{
    if (!worker_) return;
    discardScan_ = true;
    worker_->cancel();
    status_->setText(tr("Indexing cancelled. The previous completed index is retained."));
    cancelButton_->setEnabled(false);
}

void MainWindow::clearLibrary()
{
    if (worker_) { discardScan_ = true; worker_->cancel(); }
    folders_.clear();
    folderList_->clear();
    saveFolders();
    model_->setLibrary({});
    indexedFiles_ = 0;
    issues_.clear();
    search_->clear();
    details_->clear();
    status_->setText(tr("Library cleared. Add a folder to begin again."));
    updateCount();
    updateControls();
}

void MainWindow::removeSelectedFolder()
{
    if (isScanning() || folderList_->currentRow() < 0) return;
    folders_.removeAt(folderList_->currentRow());
    delete folderList_->takeItem(folderList_->currentRow());
    saveFolders();
    // Never retain matches from a removed root if reindexing is cancelled.
    model_->setLibrary({});
    indexedFiles_ = 0;
    issues_.clear();
    if (folders_.isEmpty()) clearLibrary(); else refreshLibrary();
    updateCount();
}

void MainWindow::updateControls()
{
    const bool scanning = isScanning();
    addButton_->setEnabled(!scanning);
    addAction_->setEnabled(!scanning);
    refreshButton_->setEnabled(!scanning && !folders_.isEmpty());
    refreshAction_->setEnabled(refreshButton_->isEnabled());
    removeButton_->setEnabled(!scanning && folderList_->currentRow() >= 0);
    cancelButton_->setVisible(scanning);
    cancelButton_->setEnabled(scanning && !discardScan_);
    issuesButton_->setVisible(!issues_.isEmpty());
    const bool selected = !selectedPath().isEmpty();
    revealButton_->setEnabled(selected);
    copyButton_->setEnabled(selected);
}

void MainWindow::updateCount()
{
    count_->setText(tr("%1 results · %2 indexed files").arg(searchModel_->rowCount()).arg(indexedFiles_));
}

QString MainWindow::selectedPath() const
{
    const auto rows = table_->selectionModel()->selectedRows();
    return rows.isEmpty() ? QString() : rows.first().data(fxb::ResultModel::PathRole).toString();
}

void MainWindow::showSelection()
{
    const auto rows = table_->selectionModel()->selectedRows();
    details_->setPlainText(rows.isEmpty() ? QString() : rows.first().data(fxb::ResultModel::DetailRole).toString());
    updateControls();
}

void MainWindow::revealSelectedFile()
{
    const QString path = selectedPath();
    if (path.isEmpty()) return;
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath())))
        status_->setText(tr("Could not open the containing folder. Use Copy path instead."));
}

void MainWindow::copySelectedPath()
{
    const QString path = selectedPath();
    if (!path.isEmpty()) QApplication::clipboard()->setText(QDir::toNativeSeparators(path));
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!isScanning() && event->mimeData()->hasUrls()) {
        for (const auto &url : event->mimeData()->urls())
            if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir()) { event->acceptProposedAction(); return; }
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (isScanning()) return;
    bool changed = false;
    for (const auto &url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) continue;
        const QString path = fxb::canonicalFolder(url.toLocalFile());
        if (path.isEmpty() || folders_.contains(path)) continue;
        folders_ += path;
        auto *item = new QListWidgetItem(path, folderList_);
        item->setToolTip(path);
        changed = true;
    }
    if (changed) { saveFolders(); refreshLibrary(); event->acceptProposedAction(); }
}
