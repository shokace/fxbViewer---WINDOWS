#include <QtTest>
#include <QAbstractItemModelTester>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTableView>
#include <QTemporaryDir>
#include <QUrl>
#include <QtEndian>
#include "core/bankreader.h"
#include "core/library.h"
#include "core/resultmodel.h"
#include "mainwindow.h"

using namespace fxb;
class Tests final : public QObject {
    Q_OBJECT
    QString root_;
    QByteArray fixture(const QString &name) const {
        QFile f(root_ + '/' + name);
        if (!f.open(QIODevice::ReadOnly)) qFatal("Missing fixture: %s", qPrintable(f.fileName()));
        return f.readAll();
    }
    static void write(const QString &path, const QByteArray &data) {
        QFile file(path);
        QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.errorString()));
        QCOMPARE(file.write(data), qint64(data.size()));
    }
    static void put32(QByteArray &bytes, int offset, quint32 value) {
        qToBigEndian<quint32>(value, reinterpret_cast<uchar *>(bytes.data() + offset));
    }
    QUrl openedUrl_;
public slots:
    void captureUrl(const QUrl &url) { openedUrl_ = url; }
private slots:
    void initTestCase() {
        root_ = QFINDTESTDATA("fixtures");
        QVERIFY(!root_.isEmpty());
        QCoreApplication::setOrganizationName("fxbviewer-tests");
        QCoreApplication::setApplicationName("fxbviewer-tests");
    }
    void referenceCorpus_data() {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("hash");
        QTest::addColumn<QString>("format");
        QTest::addColumn<QString>("plugin");
        QTest::addColumn<QStringList>("names");
        const auto manifest = QJsonDocument::fromJson(fixture("manifest.json")).array();
        QCOMPARE(manifest.size(), 12);
        for (const auto &value : manifest) {
            const auto o = value.toObject();
            QStringList names;
            for (const auto &n : o["names"].toArray()) names += n.toString();
            QTest::newRow(qPrintable(o["file"].toString())) << o["file"].toString() << o["sha256"].toString()
                << o["format"].toString() << o["plugin"].toString() << names;
        }
    }
    void referenceCorpus() {
        QFETCH(QString, file); QFETCH(QString, hash); QFETCH(QString, format); QFETCH(QString, plugin); QFETCH(QStringList, names);
        const QByteArray bytes = fixture(file);
        QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()), hash);
        const auto doc = BankReader::parse(bytes);
        QVERIFY2(doc.isValid(), qPrintable(doc.error));
        QCOMPARE(doc.format, format);
        QCOMPARE(doc.pluginId, plugin);
        QCOMPARE(doc.presets.size(), names.size());
        for (int i = 0; i < names.size(); ++i) {
            QCOMPARE(doc.presets[i].name, names[i]);
            QCOMPARE(doc.presets[i].number, i + 1);
        }
    }
    void realWorldSizeException() {
        const auto doc = BankReader::parse(fixture("reference/podolski-pad.fxp"));
        QVERIFY(doc.isValid());
        QVERIFY(doc.warnings.join(' ').contains("unspecified"));
        QVERIFY(doc.embeddedText.contains("Podolski"));
    }
    void embeddedBinaryText() {
        const auto doc = BankReader::parse(fixture("generated/opaque.fxb"));
        QVERIFY(doc.isValid());
        QVERIFY(doc.presets.isEmpty());
        for (const QString name : {"Hidden Lead", "Deep Bass", "Crystal Keys", "Silver Bell"})
            QVERIFY2(doc.embeddedText.contains(name), qPrintable(name));
        QVERIFY(!doc.warnings.isEmpty());
        const auto real = BankReader::parse(fixture("reference/massive-test-bank.FXB"));
        QVERIFY(real.embeddedText.contains("Untitled"));
        QVERIFY(real.embeddedText.contains("Massive"));
    }
    void allTruncationsRejected() {
        for (const QString file : {"generated/standard.fxb", "generated/parameter.fxp", "generated/chunk.fxp", "generated/opaque.fxb"}) {
            const auto bytes = fixture(file);
            for (int length = 0; length < bytes.size(); ++length)
                QVERIFY2(!BankReader::parse(bytes.left(length)).isValid(), qPrintable(file + " length " + QString::number(length)));
        }
    }
    void corruptHeadersAndCounts() {
        const auto original = fixture("generated/standard.fxb");
        auto b = original; b[0] = 'X'; QVERIFY(!BankReader::parse(b).isValid());
        b = original; b.replace(8, 4, "FAIL"); QVERIFY(!BankReader::parse(b).isValid());
        b = original; put32(b, 12, 99); QVERIFY(!BankReader::parse(b).isValid());
        b = original; put32(b, 4, 0xffffffff); QVERIFY(!BankReader::parse(b).isValid());
        b = original; put32(b, 24, 0xffffffff); QVERIFY(!BankReader::parse(b).isValid());
        b = original; put32(b, 156 + 24, 0xffffffff); QVERIFY(!BankReader::parse(b).isValid());
        b = original; b.replace(156 + 8, 4, "FxBk"); QVERIFY(!BankReader::parse(b).isValid());
        b = original + "garbage"; QVERIFY(!BankReader::parse(b).isValid());
        b = fixture("generated/opaque.fxb"); put32(b, 156, 0xffffffff); QVERIFY(!BankReader::parse(b).isValid());
        b = fixture("generated/chunk.fxp"); put32(b, 56, 0xffffffff); QVERIFY(!BankReader::parse(b).isValid());
    }
    void textLimitIsVisible() {
        QByteArray b = fixture("generated/opaque.fxb").left(160);
        const QByteArray payload(BankReader::MaxTextCharacters + 100, 'A');
        b += payload; put32(b, 4, b.size() - 8); put32(b, 156, payload.size());
        const auto doc = BankReader::parse(b);
        QVERIFY(doc.isValid());
        QVERIFY(doc.embeddedText.size() <= BankReader::MaxTextCharacters);
        QVERIFY(doc.warnings.join(' ').contains("limit reached"));
    }
    void longTextDoesNotSplitMatches() {
        auto b = fixture("generated/opaque.fxb").left(160);
        const QByteArray payload = QByteArray(4094, 'A') + "Glass Lead" + QByteArray(100, 'B');
        b += payload; put32(b, 4, b.size() - 8); put32(b, 156, payload.size());
        const auto doc = BankReader::parse(b);
        QVERIFY(doc.isValid());
        QVERIFY(doc.embeddedText.contains("Glass Lead"));
    }
    void indexingLimitsAreReported() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        write(dir.filePath("a.fxb"), fixture("generated/standard.fxb"));
        write(dir.filePath("b.fxb"), fixture("generated/standard.fxb"));
        std::atomic_bool stop{false};
        auto result = scanFolders({dir.path()}, stop, {}, {2, 4, 1000});
        QCOMPARE(result.entries.size(), 2);
        QVERIFY(result.issues.isEmpty()); // Exactly at the limit is complete.
        result = scanFolders({dir.path()}, stop, {}, {1, 4, 1000});
        QCOMPARE(result.entries.size(), 1);
        QVERIFY(result.issues.join(' ').contains("file limit reached"));
        result = scanFolders({dir.path()}, stop, {}, {2, 2, 1000});
        QCOMPARE(result.entries.size(), 1);
        QVERIFY(result.issues.join(' ').contains("memory limit reached"));
        write(dir.filePath("a.fxb"), fixture("generated/opaque.fxb"));
        result = scanFolders({dir.path()}, stop, {}, {2, 4, 1});
        QVERIFY(result.entries.isEmpty());
        QVERIFY(result.issues.join(' ').contains("memory limit reached"));
    }
    void boundedFileReading() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        QVERIFY(!BankReader::readFile(dir.filePath("missing.fxb")).isValid());
        QFile file(dir.filePath("large.fxb")); QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(BankReader::MaxFileBytes + 1)); file.close();
        QVERIFY(BankReader::readFile(file.fileName()).error.contains("64 MiB"));
    }
    void malformedMutationSmoke() {
        const auto original = fixture("generated/standard.fxb");
        for (int i = 0; i < original.size(); ++i) {
            for (const unsigned char value : {0, 255}) {
                auto changed = original; changed[i] = char(value);
                const auto doc = BankReader::parse(changed);
                QVERIFY(!doc.isValid() || doc.presets.size() <= 2);
            }
        }
    }
    void recursiveDeduplicatedScan() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir("nested"));
        for (int i = 0; i < 25; ++i)
            write(dir.filePath(QString("nested/bank%1.FXB").arg(i)), fixture("generated/standard.fxb"));
        write(dir.filePath("patch.FXP"), fixture("generated/parameter.fxp"));
        write(dir.filePath("ignore.txt"), fixture("generated/parameter.fxp"));
        write(dir.filePath("broken.fxb"), "garbage");
        std::atomic_bool stop{false};
        int finalDone = -1, finalTotal = -1;
        const auto result = scanFolders({dir.path(), dir.path() + "/nested", dir.path()}, stop,
            [&](int done, int total) { finalDone = done; finalTotal = total; });
        QCOMPARE(result.discovered, 27);
        QCOMPARE(result.entries.size(), 26);
        QCOMPARE(result.issues.size(), 1);
        QCOMPARE(finalDone, 27); QCOMPARE(finalTotal, 27);
        QSet<QString> paths;
        for (const auto &e : result.entries) { QVERIFY(!e.path.isEmpty()); paths.insert(e.path); }
        QCOMPARE(paths.size(), 26);
    }
    void symlinkAndEmptyAndMissingFolders() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        std::atomic_bool stop{false};
        QCOMPARE(scanFolders({dir.path()}, stop).discovered, 0);
        QVERIFY(!scanFolders({dir.filePath("missing")}, stop).issues.isEmpty());
        write(dir.filePath("bank.fxb"), fixture("generated/standard.fxb"));
#ifndef Q_OS_WIN
        QVERIFY(QFile::link(dir.filePath("bank.fxb"), dir.filePath("alias.fxb")));
        QVERIFY(QFile::link(dir.path(), dir.filePath("loop")));
#endif
        QCOMPARE(scanFolders({dir.path()}, stop).entries.size(), 1);
    }
    void cancelBeforeAndDuringScan() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        std::atomic_bool stop{true};
        QVERIFY(scanFolders({dir.path()}, stop).cancelled);
        for (int i = 0; i < 30; ++i) write(dir.filePath(QString("%1.fxb").arg(i)), fixture("generated/standard.fxb"));
        stop = false;
        const auto result = scanFolders({dir.path()}, stop, [&](int done, int) { if (done > 0) stop = true; });
        QVERIFY(result.cancelled);
        QVERIFY(result.entries.size() < 30);
    }
    void searchModelAndSorting() {
        ResultModel model;
        QAbstractItemModelTester checker(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        SearchModel search; search.setSourceModel(&model);
        QAbstractItemModelTester proxyChecker(&search, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setLibrary({{"/library/Bank.fxb", BankReader::parse(fixture("generated/standard.fxb"))},
                          {"/library/Opaque.fxb", BankReader::parse(fixture("generated/opaque.fxb"))}});
        QCOMPARE(search.rowCount(), 3);
        search.setQuery("  gLAss LEAD  "); QCOMPARE(search.rowCount(), 1);
        search.setQuery("Hidden Lead"); QCOMPARE(search.rowCount(), 1);
        QVERIFY(search.index(0, 0).data().toString().contains("names unavailable"));
        search.setQuery("Crystal Keys"); QCOMPARE(search.rowCount(), 1);
        search.setQuery("no-match"); QCOMPARE(search.rowCount(), 0);
        search.setQuery("[.*]"); QCOMPARE(search.rowCount(), 0); // Literal search, not regex.
        search.setQuery("BANK.FXB"); QCOMPARE(search.rowCount(), 2);
        search.setQuery("Test"); QCOMPARE(search.rowCount(), 3);
        search.setQuery(" "); QCOMPARE(search.rowCount(), 3);
        search.sort(0, Qt::DescendingOrder);
        QVERIFY(!search.index(0, 0).data(ResultModel::PathRole).toString().isEmpty());
        model.setLibrary({}); QCOMPARE(search.rowCount(), 0);
    }
    void referenceLibraryQueries() {
        std::atomic_bool stop{false};
        const auto result = scanFolders({root_ + "/reference"}, stop);
        QCOMPARE(result.discovered, 5);
        QCOMPARE(result.entries.size(), 5);
        QVERIFY(result.issues.isEmpty());
        ResultModel model;
        model.setLibrary(result.entries);
        SearchModel search;
        search.setSourceModel(&model);
        QCOMPARE(search.rowCount(), 5);
        search.setQuery("Untitled"); QCOMPARE(search.rowCount(), 2);
        search.setQuery("Podolski"); QCOMPARE(search.rowCount(), 1);
        search.setQuery("recm"); QCOMPARE(search.rowCount(), 2);
    }
    void uiSearchClipboardRevealAndClear() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString bankPath = dir.filePath("bank.fxb");
        write(bankPath, fixture("generated/standard.fxb"));
        MainWindow window(dir.filePath("settings.ini")); window.show();
        auto *table = window.findChild<QTableView *>("resultsTable");
        auto *search = window.findChild<QLineEdit *>("searchInput");
        QVERIFY(table); QVERIFY(search);
        QVERIFY(window.addFolder(dir.path()));
        QTRY_VERIFY_WITH_TIMEOUT(!window.isScanning(), 5000);
        QCOMPARE(table->model()->rowCount(), 2);
        QVERIFY(!window.addFolder(dir.path() + "/."));
        QCOMPARE(window.folders().size(), 1);
        search->setText("glass"); QCOMPARE(table->model()->rowCount(), 1);
        search->setText("bass"); QCOMPARE(table->model()->rowCount(), 1);
        search->setText("glass"); QCOMPARE(table->model()->rowCount(), 1);
        search->clear(); QCOMPARE(table->model()->rowCount(), 2);
        table->selectRow(0);
        QVERIFY(window.findChild<QPushButton *>("copyPath")->isEnabled());
        window.copySelectedPath();
        QCOMPARE(QApplication::clipboard()->text(), QDir::toNativeSeparators(bankPath));
        openedUrl_.clear();
        QDesktopServices::setUrlHandler("file", this, "captureUrl");
        window.revealSelectedFile();
        QDesktopServices::unsetUrlHandler("file");
        QCOMPARE(openedUrl_, QUrl::fromLocalFile(dir.path()));
        QTest::mouseClick(window.findChild<QPushButton *>("clearLibrary"), Qt::LeftButton);
        QCOMPARE(table->model()->rowCount(), 0);
        QVERIFY(window.folders().isEmpty());
        QVERIFY(!window.findChild<QPushButton *>("revealFile")->isEnabled());
        QVERIFY(QFile::exists(bankPath));
        QSettings saved(dir.filePath("settings.ini"), QSettings::IniFormat);
        QVERIFY(saved.value("library/folders").toStringList().isEmpty());
    }
    void uiPersistenceRefreshRemoveAndCancel() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString settings = dir.filePath("settings.ini");
        const QString bank = dir.filePath("bank.fxb");
        write(bank, fixture("generated/standard.fxb"));
        {
            MainWindow window(settings);
            QVERIFY(window.addFolder(dir.path()));
            QTRY_VERIFY_WITH_TIMEOUT(!window.isScanning(), 5000);
        }
        MainWindow window(settings);
        auto *table = window.findChild<QTableView *>("resultsTable");
        QTRY_COMPARE_WITH_TIMEOUT(table->model()->rowCount(), 2, 5000);
        QCOMPARE(window.folders(), QStringList{dir.path()});
        window.refreshLibrary(); window.cancelScan();
        QTRY_VERIFY_WITH_TIMEOUT(!window.isScanning(), 5000);
        QCOMPARE(table->model()->rowCount(), 2);
        QVERIFY(QFile::remove(bank));
        window.refreshLibrary();
        QTRY_VERIFY_WITH_TIMEOUT(!window.isScanning(), 5000);
        QCOMPARE(table->model()->rowCount(), 0);
        window.findChild<QListWidget *>("folderList")->setCurrentRow(0);
        window.removeSelectedFolder();
        QVERIFY(window.folders().isEmpty());
    }
    void uiClearAndCloseWhileScanning() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        for (int i = 0; i < 80; ++i) write(dir.filePath(QString("%1.fxb").arg(i)), fixture("generated/standard.fxb"));
        {
            MainWindow window(dir.filePath("settings.ini"));
            QVERIFY(window.addFolder(dir.path()));
            window.clearLibrary();
            QTRY_VERIFY_WITH_TIMEOUT(!window.isScanning(), 5000);
            QCOMPARE(window.findChild<QTableView *>("resultsTable")->model()->rowCount(), 0);
            QVERIFY(window.folders().isEmpty());
        }
        {
            MainWindow window(dir.filePath("settings.ini"));
            QVERIFY(window.addFolder(dir.path()));
            // Destructor must cancel/join, never destroy a running QThread.
        }
    }
    void uiDropAndErrorReporting() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir("one"));
        QVERIFY(QDir(dir.path()).mkdir("two"));
        write(dir.filePath("one/bank.fxb"), fixture("generated/standard.fxb"));
        write(dir.filePath("two/bad.fxb"), "broken");
        MainWindow window(dir.filePath("settings.ini")); window.show();
        QVERIFY(!window.addFolder(dir.filePath("missing")));
        QVERIFY(window.folders().isEmpty());
        QVERIFY(window.findChild<QLabel *>("statusText")->text().contains("unavailable"));
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(dir.filePath("one")), QUrl::fromLocalFile(dir.filePath("two")),
                      QUrl::fromLocalFile(dir.filePath("one")), QUrl("https://example.com")});
        QDragEnterEvent enter(QPoint(50, 50), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&window, &enter); QVERIFY(enter.isAccepted());
        QDropEvent drop(QPointF(50, 50), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&window, &drop); QVERIFY(drop.isAccepted());
        QTRY_VERIFY_WITH_TIMEOUT(!window.isScanning(), 5000);
        QCOMPARE(window.folders().size(), 2);
        QCOMPARE(window.findChild<QTableView *>("resultsTable")->model()->rowCount(), 2);
        QVERIFY(!window.findChild<QPushButton *>("viewIssues")->isHidden());
        QVERIFY(window.findChild<QLabel *>("statusText")->text().contains("1 of 2"));
    }
    void uiResponsiveAndScalable() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        for (int i = 0; i < 100; ++i) write(dir.filePath(QString("%1.fxb").arg(i)), fixture("generated/standard.fxb"));
        MainWindow window(dir.filePath("settings.ini"));
        QFont large = window.font(); large.setPointSize(18); window.setFont(large);
        window.resize(1280, 900); window.show();
        QVERIFY(window.addFolder(dir.path()));
        auto *search = window.findChild<QLineEdit *>("searchInput");
        QTest::keyClicks(search, "glass");
        QCOMPARE(search->text(), QString("glass"));
        QTRY_VERIFY_WITH_TIMEOUT(!window.isScanning(), 5000);
        QCOMPARE(window.findChild<QTableView *>("resultsTable")->model()->rowCount(), 100);
        auto *button = window.findChild<QPushButton *>("refreshLibrary");
        QVERIFY(button->width() >= button->sizeHint().width());
        QVERIFY(button->height() >= button->sizeHint().height());
        window.resize(1500, 1000);
        QCoreApplication::processEvents();
        QVERIFY(window.findChild<QTableView *>("resultsTable")->width() > 600);
        const QString screenshot = qEnvironmentVariable("FXBVIEWER_TEST_SCREENSHOT");
        if (!screenshot.isEmpty()) QVERIFY(window.grab().save(screenshot));
    }
};
QTEST_MAIN(Tests)
#include "test_fxbviewer.moc"
