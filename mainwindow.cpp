#include "mainwindow.h"
#include "core/resultmodel.h"
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QComboBox>
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
#include <QSysInfo>
#include <QSplitter>
#include <QStandardPaths>
#include <QTableView>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(const QString &settingsPath, QWidget *parent, bool demoMode) : QMainWindow(parent), demoMode_(demoMode)
{
    settings_ = settingsPath.isEmpty() ? std::make_unique<QSettings>()
        : std::make_unique<QSettings>(settingsPath, QSettings::IniFormat);
    setWindowTitle(demoMode_ ? tr("FXB Viewer — Demo") : tr("FXB Viewer"));
    setAcceptDrops(true);
    resize(1240, 840);
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
    title->setObjectName("appTitle");
    layout->addWidget(title);
    auto *subtitle = new QLabel(tr("Remember the sound. Find the preset."));
    subtitle->setWordWrap(true);
    subtitle->setObjectName("subtitle");
    layout->addWidget(subtitle);
    if (demoMode_) {
        auto *demo = new QLabel(tr("DEMO LIBRARY  ·  Synthetic presets  ·  Your own library stays separate"));
        demo->setObjectName("demoBanner");
        demo->setWordWrap(true);
        layout->addWidget(demo);
    }

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

    auto *splitter = librarySplit_ = new QSplitter;
    auto *sidebar = new QWidget;
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 0, 8, 0);
    sideLayout->addWidget(new QLabel(tr("LIBRARY FOLDERS")));
    folderList_ = new QListWidget;
    folderList_->setObjectName("folderList");
    folderList_->setAccessibleName(tr("Library folders"));
    folderList_->setMinimumWidth(180);
    sideLayout->addWidget(folderList_);
    removeButton_ = new QPushButton(tr("Remove folder"));
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
    search_->setPlaceholderText(tr("Try a name, a few remembered words, or a bank…"));
    search_->setClearButtonEnabled(true);
    search_->setMaxLength(256);
    search_->setAccessibleName(tr("Search library"));
    contentLayout->addWidget(search_);
    auto *searchHint = new QLabel(tr("Any word order. Accents optional. Use “quotes” for a phrase."));
    searchHint->setObjectName("searchHint");
    searchHint->setWordWrap(true);
    contentLayout->addWidget(searchHint);
    auto *options = new QHBoxLayout;
    namesOnly_ = new QCheckBox(tr("Names only"));
    namesOnly_->setObjectName("namesOnly");
    typos_ = new QCheckBox(tr("Find similar names"));
    typos_->setObjectName("allowTypos");
    typos_->setChecked(true);
    typos_->setToolTip(tr("If no exact matches are found, try one-letter spelling differences in preset names."));
    sortChoice_ = new QComboBox;
    sortChoice_->setObjectName("sortChoice");
    sortChoice_->setAccessibleName(tr("Sort results"));
    sortChoice_->addItems({tr("Best matches"), tr("Preset A–Z"), tr("Bank A–Z")});
    options->addWidget(namesOnly_);
    options->addWidget(typos_);
    options->addStretch();
    options->addWidget(sortChoice_);
    contentLayout->addLayout(options);
    if (demoMode_) {
        auto *examples = new QHBoxLayout;
        examples->addWidget(new QLabel(tr("Try:")));
        for (const QString query : {"glass lead", "lead glass", "glass leed", "hidden lead"}) {
            auto *example = new QPushButton(query);
            example->setProperty("example", true);
            connect(example, &QPushButton::clicked, this, [this, query] { search_->setText(query); });
            examples->addWidget(example);
        }
        examples->addStretch();
        contentLayout->addLayout(examples);
    }
    count_ = new QLabel;
    count_->setObjectName("resultCount");
    count_->setWordWrap(true);
    contentLayout->addWidget(count_);
    model_ = new fxb::ResultModel(this);
    searchModel_ = new fxb::SearchModel(this);
    searchModel_->setSourceModel(model_);
    table_ = new QTableView;
    table_->setObjectName("resultsTable");
    table_->setAccessibleName(tr("Preset search results"));
    table_->setModel(searchModel_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSortingEnabled(false);
    searchModel_->sort(0, Qt::AscendingOrder);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(false);
    table_->verticalHeader()->hide();
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setColumnWidth(0, 280);
    table_->setColumnWidth(1, 200);
    table_->setColumnWidth(2, 130);
    table_->setColumnWidth(3, 100);
    emptyState_ = new QLabel;
    emptyState_->setObjectName("emptyState");
    emptyState_->setWordWrap(true);
    contentLayout->addWidget(emptyState_);
    auto *resultsSplit = resultsSplit_ = new QSplitter(Qt::Vertical);
    resultsSplit->addWidget(table_);
    details_ = new QPlainTextEdit;
    details_->setObjectName("resultDetails");
    details_->setAccessibleName(tr("Selected result details"));
    details_->setReadOnly(true);
    details_->setMinimumHeight(110);
    details_->setMaximumBlockCount(1500);
    details_->setPlaceholderText(tr("Select a result to inspect its location and format details."));
    resultsSplit->addWidget(details_);
    resultsSplit->setStretchFactor(0, 3);
    resultsSplit->setStretchFactor(1, 1);
    resultsSplit->setSizes({360, 140});
    contentLayout->addWidget(resultsSplit, 1);
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
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    issuesButton_ = new QPushButton(tr("Review notices"));
    issuesButton_->setObjectName("viewIssues");
    statusLine->addWidget(status_, 1);
    statusLine->addWidget(issuesButton_);
    layout->addLayout(statusLine);
    settingsWarning_ = new QLabel;
    settingsWarning_->setObjectName("settingsWarning");
    settingsWarning_->setTextFormat(Qt::PlainText);
    settingsWarning_->setWordWrap(true);
    settingsWarning_->hide();
    layout->addWidget(settingsWarning_);
    setCentralWidget(central);
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QDialog { background: #101923; }
        QWidget { color: #e1eaf2; }
        QLabel#subtitle, QLabel#searchHint, QLabel#resultCount { color: #9fbbca; }
        QLabel#demoBanner { background: #16392f; color: #8ee5c8; padding: 8px 12px; border-radius: 6px; }
        QLabel#emptyState { color: #a8bccb; padding: 10px; }
        QMenuBar { background: #101923; }
        QMenuBar::item:selected, QMenu::item:selected { background: #2b4659; }
        QMenu { background: #1b2a39; border: 1px solid #344b60; }
        QLineEdit, QPlainTextEdit, QListWidget, QTableView {
            background: #152332; border: 1px solid #2b4153; border-radius: 6px;
            selection-background-color: #28584f; selection-color: #f2fffa;
        }
        QLineEdit { padding: 10px 12px; }
        QLineEdit:focus { border: 1px solid #68d8b7; }
        QPlainTextEdit { padding: 10px; }
        QListWidget::item { padding: 8px; }
        QTableView { alternate-background-color: #192a3b; }
        QHeaderView::section { background: #203246; color: #aac4d6; padding: 8px; border: 0; border-bottom: 1px solid #314c61; }
        QPushButton { background: #22384c; border: 1px solid #36536a; border-radius: 6px; padding: 8px 12px; }
        QPushButton:hover { background: #2e4a62; }
        QPushButton:pressed { background: #173d38; }
        QPushButton:disabled { color: #758897; background: #1b2936; border-color: #293b49; }
        QPushButton#addFolder { background: #6ad8b9; color: #102c26; border-color: #6ad8b9; font-weight: bold; }
        QPushButton#clearLibrary { background: transparent; color: #9eb7c8; }
        QPushButton[example="true"] { padding: 5px 8px; color: #91e0c7; }
        QComboBox { background: #203448; border: 1px solid #36536a; border-radius: 6px; padding: 6px 12px; }
        QComboBox QAbstractItemView { background: #203448; selection-background-color: #356050; }
        QCheckBox { spacing: 7px; }
        QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #5b798c; border-radius: 3px; background: #172737; }
        QCheckBox::indicator:checked { background: #6ad8b9; border-color: #6ad8b9; image: url(:/check.png); }
        QProgressBar { border: 1px solid #36536a; border-radius: 4px; background: #152332; text-align: center; }
        QProgressBar::chunk { background: #397e6a; }
        QSplitter::handle { background: #203246; }
        QScrollBar:vertical { background: #152332; width: 12px; margin: 0; }
        QScrollBar:horizontal { background: #152332; height: 12px; margin: 0; }
        QScrollBar::handle { background: #405c70; border-radius: 4px; min-height: 24px; min-width: 24px; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: none; }
        QToolTip { background: #203448; color: #e1eaf2; border: 1px solid #47627a; }
    )"));

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    addAction_ = fileMenu->addAction(tr("Add folder…"));
    addAction_->setShortcut(QKeySequence::Open);
    connect(addAction_, &QAction::triggered, this, &MainWindow::chooseFolder);
    refreshAction_ = fileMenu->addAction(tr("Refresh library"));
    refreshAction_->setShortcut(QKeySequence::Refresh);
    connect(refreshAction_, &QAction::triggered, this, &MainWindow::refreshLibrary);
    rebuildAction_ = fileMenu->addAction(tr("Rebuild from disk"));
    rebuildAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    connect(rebuildAction_, &QAction::triggered, this, &MainWindow::rebuildLibrary);
    refreshButton_->setToolTip(tr("Reuse unchanged banks. Use File → Rebuild from disk to bypass the cache."));
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
        QMessageBox::information(this, tr("Find a preset"), tr("Add folders containing FXB banks or FXP presets. Indexing runs in the background; search ranks the completed library as you type. Words can appear in any order; use quotes for a phrase. Accents and case are ignored. If no exact results exist, similar-name search can recover one-letter mistakes. Names rank ahead of bank metadata. Refresh reuses banks whose size and timestamps have not changed; File → Rebuild from disk rereads everything.\n\nStandard banks show preset names and positions. Plugin-specific banks expose searchable embedded text where available; those matches are not guaranteed preset names. Compressed or encrypted names cannot be decoded.\n\nSelect a result to copy its path or open its folder. Cancel keeps the previous completed index. Clear library removes folder selections and results, never your files."));
    });
    connect(helpMenu->addAction(tr("About")), &QAction::triggered, this, [this] {
        QMessageBox::about(this, tr("About FXB Viewer"), tr("FXB Viewer %1\nSound-bank search and preset discovery.\n\nOriginal project by Shokace / Petar Juric.\nThis application reads preset files; it does not load plugins or play audio.").arg(QStringLiteral(FXBVIEWER_VERSION)));
    });
    connect(helpMenu->addAction(tr("About Qt")), &QAction::triggered, qApp, &QApplication::aboutQt);
    connect(helpMenu->addAction(tr("Copy diagnostic information")), &QAction::triggered, this, [this] {
        QApplication::clipboard()->setText(tr("FXB Viewer %1\nQt %2\nPlatform: %3 (%4)\nIndexed files: %5\nNotices: %6")
            .arg(QStringLiteral(FXBVIEWER_VERSION), QString::fromLatin1(qVersion()), QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture())
            .arg(indexedFiles_).arg(issues_.size()));
        status_->setText(tr("Diagnostic information copied. No folder paths or preset names are included."));
    });
    connect(addButton_, &QPushButton::clicked, this, &MainWindow::chooseFolder);
    connect(refreshButton_, &QPushButton::clicked, this, &MainWindow::refreshLibrary);
    connect(cancelButton_, &QPushButton::clicked, this, &MainWindow::cancelScan);
    connect(clear, &QPushButton::clicked, this, &MainWindow::clearLibrary);
    connect(removeButton_, &QPushButton::clicked, this, &MainWindow::removeSelectedFolder);
    connect(folderList_, &QListWidget::currentRowChanged, this, &MainWindow::updateControls);
    connect(search_, &QLineEdit::textChanged, this, &MainWindow::updateSearch);
    connect(namesOnly_, &QCheckBox::toggled, this, &MainWindow::updateSearch);
    connect(typos_, &QCheckBox::toggled, this, &MainWindow::updateSearch);
    connect(sortChoice_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int choice) {
        searchModel_->setRelevanceSort(choice == 0);
        searchModel_->sort(choice == 2 ? 1 : 0, Qt::AscendingOrder);
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
    for (const auto &folder : folders_) {
        auto *item = new QListWidgetItem(QFileInfo(folder).fileName().isEmpty() ? folder : QFileInfo(folder).fileName(), folderList_);
        item->setToolTip(folder);
    }
    restoreGeometry(settings_->value("window/geometry").toByteArray());
    librarySplit_->restoreState(settings_->value("window/librarySplit").toByteArray());
    resultsSplit_->restoreState(settings_->value("window/resultsSplit").toByteArray());
    table_->horizontalHeader()->restoreState(settings_->value("window/columns").toByteArray());
    namesOnly_->setChecked(settings_->value("search/namesOnly", false).toBool());
    typos_->setChecked(settings_->value("search/allowTypos", true).toBool());
    sortChoice_->setCurrentIndex(qBound(0, settings_->value("search/sort", 0).toInt(), 2));
    checkSettings();
    updateCount();
    updateControls();
    if (!folders_.isEmpty()) QTimer::singleShot(0, this, &MainWindow::refreshLibrary);
}

MainWindow::~MainWindow()
{
    savePreferences();
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
    checkSettings();
}

void MainWindow::checkSettings()
{
    const bool failed = settings_->status() != QSettings::NoError;
    settingsWarning_->setVisible(failed);
    if (failed) settingsWarning_->setText(tr("Preferences could not be read or saved. This session still works, but changes may not survive restart. Check permissions or reset the settings file: %1").arg(settings_->fileName()));
}

void MainWindow::savePreferences()
{
    settings_->setValue("window/geometry", saveGeometry());
    settings_->setValue("window/librarySplit", librarySplit_->saveState());
    settings_->setValue("window/resultsSplit", resultsSplit_->saveState());
    settings_->setValue("window/columns", table_->horizontalHeader()->saveState());
    settings_->setValue("search/namesOnly", namesOnly_->isChecked());
    settings_->setValue("search/allowTypos", typos_->isChecked());
    settings_->setValue("search/sort", sortChoice_->currentIndex());
    settings_->sync();
}

bool MainWindow::addFolder(const QString &path)
{
    if (isScanning()) return false;
    const QString canonical = fxb::canonicalFolder(path);
    if (canonical.isEmpty()) { status_->setText(tr("Folder is unavailable: %1").arg(path)); return false; }
    if (folders_.contains(canonical)) { status_->setText(tr("That folder is already in the library.")); return false; }
    folders_ += canonical;
    auto *item = new QListWidgetItem(QFileInfo(canonical).fileName().isEmpty() ? canonical : QFileInfo(canonical).fileName(), folderList_);
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

void MainWindow::refreshLibrary() { startScan(false); }
void MainWindow::rebuildLibrary() { startScan(true); }

void MainWindow::startScan(bool forceRead)
{
    if (isScanning() || folders_.isEmpty()) return;
    discardScan_ = false;
    worker_ = new fxb::ScanWorker(folders_, this, model_->searchIndex(), forceRead);
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
            if (model_->searchIndex() != result.index) model_->setIndex(result.index);
            indexedFiles_ = result.entries.size();
            issues_ = result.issues;
            for (const auto &entry : result.entries)
                for (const auto &warning : entry.bank.warnings)
                    issues_ += entry.path + QStringLiteral(": ") + warning;
            QString summary = result.discovered == 0 ? tr("No .fxb or .fxp files found in these folders.")
                : tr("Indexed %1 of %2 files · %3 notice(s)").arg(indexedFiles_).arg(result.discovered).arg(issues_.size());
            if (demoMode_) summary += tr(" · %1 reused · %2 read · %3 ms").arg(result.cachedFiles).arg(result.filesRead).arg(result.elapsedMs);
            status_->setText(summary);
        }
        worker_ = nullptr;
        completed->deleteLater();
        progress_->hide();
        updateCount();
        if (table_->selectionModel()->selectedRows().isEmpty() && searchModel_->rowCount() > 0) table_->selectRow(0);
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
    rebuildAction_->setEnabled(refreshButton_->isEnabled());
    removeButton_->setEnabled(!scanning && folderList_->currentRow() >= 0);
    cancelButton_->setVisible(scanning);
    cancelButton_->setEnabled(scanning && !discardScan_);
    issuesButton_->setVisible(!issues_.isEmpty());
    const bool selected = !selectedPath().isEmpty();
    revealButton_->setEnabled(selected);
    copyButton_->setEnabled(selected);
}

void MainWindow::updateSearch()
{
    searchModel_->setOptions({namesOnly_->isChecked(), typos_->isChecked()});
    searchModel_->setQuery(search_->text());
    updateCount();
    if (searchModel_->rowCount() > 0) table_->selectRow(0);
    showSelection();
}

void MainWindow::updateCount()
{
    const auto stats = searchModel_->stats();
    QString text = tr("%1 results · %2 indexed files").arg(searchModel_->rowCount()).arg(indexedFiles_);
    if (stats.usedTypos) text += tr(" · Similar names — no exact matches");
    if (demoMode_ && !search_->text().isEmpty()) text += tr(" · %1 ms search").arg(stats.elapsedNanoseconds / 1e6, 0, 'f', 2);
    count_->setText(text);
    emptyState_->setVisible(searchModel_->rowCount() == 0);
    emptyState_->setText(model_->rowCount() == 0 ? tr("Your sounds will appear here. Add a folder to get started.")
        : tr("No matches. Try fewer words, enable similar names, or search all bank text."));
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
        auto *item = new QListWidgetItem(QFileInfo(path).fileName().isEmpty() ? path : QFileInfo(path).fileName(), folderList_);
        item->setToolTip(path);
        changed = true;
    }
    if (changed) { saveFolders(); refreshLibrary(); event->acceptProposedAction(); }
}
