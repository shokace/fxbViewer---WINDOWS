#include <QtTest>
#include <QAbstractItemModelTester>
#include <QClipboard>
#include <QCheckBox>
#include <QComboBox>
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
#include <QPlainTextEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QTableView>
#include <QTemporaryDir>
#include <QUrl>
#include <QtEndian>
#include <random>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif
#include "core/bankreader.h"
#include "core/searchindex.h"
#include "core/demodata.h"
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
    void rankedNamesAndUnorderedWords() {
        auto bank = BankReader::parse(fixture("generated/standard.fxb"));
        bank.presets = {{"Glass Lead", 1}, {"Lead Glass Harmonics", 2}, {"Glass Lead Wide", 3}};
        auto metadata = bank; metadata.presets = {{"Unrelated", 1}};
        ResultModel model;
        model.setLibrary({{"/banks/Aurora.fxb", bank}, {"/banks/Glass Lead.fxb", metadata}});
        SearchModel search; search.setSourceModel(&model);
        search.setQuery("glass lead");
        QCOMPARE(search.rowCount(), 4);
        QCOMPARE(search.index(0, 0).data().toString(), QString("1. Glass Lead"));
        QCOMPARE(search.index(3, 0).data(ResultModel::MatchRole).toString(), QString("Bank name"));
        search.setQuery("lead glass"); QCOMPARE(search.rowCount(), 4);
        search.setQuery("\"glass lead\""); QCOMPARE(search.rowCount(), 3);
        search.setQuery(QString::fromUtf8("“glass lead”")); QCOMPARE(search.rowCount(), 3);
        search.setOptions({true, true}); QCOMPARE(search.rowCount(), 2);
        search.setQuery("etoile"); QCOMPARE(search.rowCount(), 0);
        model.setLibrary({{"/banks/Unicode.fxp", BankReader::parse(fixture("generated/unicode.fxp"))}});
        search.setQuery("  ETOILE pad  "); QCOMPARE(search.rowCount(), 1);
        QVERIFY(search.index(0, 0).data().toString().contains(QString::fromUtf8("Étoile")));
    }
    void typoRecovery_data() {
        QTest::addColumn<QString>("query");
        QTest::newRow("missing-letter") << "glss lead";
        QTest::newRow("extra-letter") << "glaass lead";
        QTest::newRow("substitution") << "glass leed";
        QTest::newRow("transposition") << "galss lead";
    }
    void typoRecovery() {
        QFETCH(QString, query);
        const auto index = SearchIndex::build({{"/banks/Aurora.fxb", BankReader::parse(fixture("generated/standard.fxb"))}});
        const auto results = index->search(query);
        QCOMPARE(results.hits.size(), 1);
        QVERIFY(results.stats.usedTypos);
        QCOMPARE(results.hits[0].source, MatchSource::SimilarName);
        QCOMPARE(index->record(results.hits[0].row).name, QString("Glass Lead"));
        QVERIFY(index->search(query, {false, false}).hits.isEmpty());
        QVERIFY(index->search('"' + query + '"').hits.isEmpty());
    }
    void conservativeTypoAndSourceLabels() {
        auto bank = BankReader::parse(fixture("generated/standard.fxb"));
        bank.presets = {{"Glass Lead", 1}, {"Glaze Lead", 2}, {"1235 Bass", 3}};
        const auto index = SearchIndex::build({{"/banks/Aurora.fxb", bank},
            {"/banks/Archive.fxb", BankReader::parse(fixture("generated/opaque.fxb"))}});
        auto hits = index->search("glass lead");
        QCOMPARE(hits.hits.size(), 1); QVERIFY(!hits.stats.usedTypos);
        QVERIFY(index->search("gzzss lead").hits.isEmpty());
        QVERIFY(index->search("zlx lead").hits.isEmpty());
        QVERIFY(index->search("1234 bass").hits.isEmpty());
        hits = index->search("hidden lead");
        QCOMPARE(hits.hits.size(), 1);
        QCOMPARE(hits.hits[0].source, MatchSource::EmbeddedText);
        QVERIFY(index->snippet(hits.hits[0]).contains("Hidden Lead"));
        QVERIFY(index->search("hidden lead", {true, true}).hits.isEmpty());
    }
    void indexedSearchMatchesIndependentLinearOracle() {
        QVector<LibraryEntry> entries;
        for (int i = 0; i < 80; ++i) {
            auto bank = BankReader::parse(fixture("generated/standard.fxb"));
            bank.presets = {{QString("Velvet Bass %1").arg(i), 1}, {QString("Glass LEAD %1").arg(i), 2}, {"Étoile Pad", 3}};
            entries += {QString("/banks/Collection_%1.fxb").arg(i), bank};
        }
        entries += {"/banks/Archive.fxb", BankReader::parse(fixture("generated/opaque.fxb"))};
        const auto index = SearchIndex::build(entries);
        for (const QString query : {"a", "ss", "glass", "velvet", "etoile", "17", "collection", "test", "crystal", "missing", "[.*]"}) {
            QSet<int> expected;
            const auto term = SearchIndex::normalize(query);
            for (int row = 0; row < index->size(); ++row) {
                const auto &entry = index->entry(row);
                const QStringList fields{index->record(row).name, QFileInfo(entry.path).fileName(), entry.path,
                    entry.bank.pluginId, entry.bank.embeddedText};
                for (const auto &field : fields) if (SearchIndex::normalize(field).contains(term)) { expected.insert(row); break; }
            }
            QSet<int> actual;
            for (const auto &hit : index->search(query, {false, false}).hits) actual.insert(hit.row);
            QCOMPARE(actual, expected);
        }
    }
    void highDiversityTextFallsBackWithoutLosingMatches() {
        BankDocument opaque;
        opaque.format = "FBCh"; opaque.pluginId = "Test";
        for (char a = 'a'; a <= 'z'; ++a) for (char b = 'a'; b <= 'z'; ++b) for (char c = 'a'; c <= 'z'; ++c)
            opaque.embeddedText += QString(QChar(a)) + QChar(b) + QChar(c) + '\n';
        opaque.embeddedText += "Unique Silver Memory";
        const auto index = SearchIndex::build({{"/banks/Diverse.fxb", opaque}});
        const auto result = index->search("silver memory");
        QCOMPARE(result.hits.size(), 1);
        QCOMPARE(result.hits[0].source, MatchSource::EmbeddedText);
        QVERIFY(index->snippet(result.hits[0]).contains("Unique Silver Memory"));
    }
    void alphabeticalSortIgnoresProgramNumbers() {
        auto bank = BankReader::parse(fixture("generated/standard.fxb"));
        bank.presets = {{"Zebra Lead", 1}, {"Amber Lead", 12}};
        ResultModel model; model.setLibrary({{"/banks/Leads.fxb", bank}});
        SearchModel search; search.setSourceModel(&model);
        search.setRelevanceSort(false); search.sort(0, Qt::AscendingOrder);
        QCOMPARE(search.index(0, 0).data().toString(), QString("12. Amber Lead"));
        search.sort(0, Qt::DescendingOrder);
        QCOMPARE(search.index(0, 0).data().toString(), QString("1. Zebra Lead"));
    }
    void selectiveQueriesPruneCandidates() {
        auto bank = BankReader::parse(fixture("generated/standard.fxb"));
        bank.presets.clear();
        for (int i = 0; i < 2000; ++i) bank.presets += {QString("Unique patch %1").arg(i, 4, 10, QLatin1Char('0')), i + 1};
        const auto index = SearchIndex::build({{"/banks/Aurora.fxb", bank}});
        const auto result = index->search("patch 0137");
        QCOMPARE(result.hits.size(), 1);
        QVERIFY(result.stats.candidatesExamined < 10);
        std::atomic_bool stop{true};
        QVERIFY(!SearchIndex::build({{"/banks/Aurora.fxb", bank}}, &stop));
    }
    void incrementalCacheRefreshAndForcedRebuild() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString first = dir.filePath("a.fxb"), second = dir.filePath("b.fxb");
        write(first, fixture("generated/standard.fxb")); write(second, fixture("generated/standard.fxb"));
        std::atomic_bool stop{false};
        const auto cold = scanFolders({dir.path()}, stop);
        QCOMPARE(cold.filesRead, 2); QCOMPARE(cold.cachedFiles, 0);
        ScanCache cache; for (const auto &entry : cold.entries) cache.insert(entry.path, entry);
        auto warm = scanFolders({dir.path()}, stop, {}, {}, cache);
        QCOMPARE(warm.filesRead, 0); QCOMPARE(warm.cachedFiles, 2);
        auto edited = fixture("generated/standard.fxb");
        edited.replace(184, 10, "Clear Lead");
        write(first, edited);
        QFile editedFile(first);
        QVERIFY(editedFile.open(QIODevice::ReadWrite));
        QVERIFY(editedFile.setFileTime(QDateTime::fromMSecsSinceEpoch(cold.entries[0].stamp.modified).addSecs(2),
                                      QFileDevice::FileModificationTime));
        editedFile.close();
        warm = scanFolders({dir.path()}, stop, {}, {}, cache);
        QCOMPARE(warm.filesRead, 1); QCOMPARE(warm.cachedFiles, 1);
        QCOMPARE(warm.entries[0].bank.presets[0].name, QString("Clear Lead"));
        write(first, fixture("generated/parameter.fxp"));
        warm = scanFolders({dir.path()}, stop, {}, {}, cache);
        QCOMPARE(warm.filesRead, 1); QCOMPARE(warm.cachedFiles, 1);
        QCOMPARE(warm.entries[0].bank.presets.size(), 1);
        const auto forced = scanFolders({dir.path()}, stop, {}, {}, cache, true);
        QCOMPARE(forced.filesRead, 2); QCOMPARE(forced.cachedFiles, 0);
        QVERIFY(QFile::remove(second));
        const auto removed = scanFolders({dir.path()}, stop, {}, {}, cache);
        QCOMPARE(removed.entries.size(), 1);
        write(first, "broken");
        const auto broken = scanFolders({dir.path()}, stop, {}, {}, cache);
        QVERIFY(broken.entries.isEmpty()); QVERIFY(!broken.issues.isEmpty());
    }
    void workerReusesUnchangedSearchSnapshot() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        write(dir.filePath("bank.fxb"), fixture("generated/standard.fxb"));
        ScanWorker first({dir.path()}); first.start();
        QVERIFY(first.wait(5000)); QVERIFY(first.result().index);
        auto index = first.result().index;
        ScanWorker second({dir.path()}, nullptr, index); second.start(); QVERIFY(second.wait(5000));
        QCOMPARE(second.result().index, index);
        QCOMPARE(second.result().filesRead, 0);
        QCOMPARE(second.result().cachedFiles, 1);
        ScanWorker forced({dir.path()}, nullptr, index, true); forced.start(); QVERIFY(forced.wait(5000));
        QVERIFY(forced.result().index != index);
        QCOMPARE(forced.result().filesRead, 1);
    }
    void demoAndSmartSearchControls() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        QString error;
        QVERIFY2(createDemoLibrary(dir.path() + "/samples", 2, 1, &error), qPrintable(error));
        QVERIFY(!createDemoLibrary(dir.path() + "/samples", 2, 1, &error)); // Never overwrite.
        MainWindow window(dir.filePath("demo.ini"), nullptr, true); window.show();
        QVERIFY(window.addFolder(dir.path() + "/samples"));
        QTRY_VERIFY_WITH_TIMEOUT(!window.isScanning(), 10000);
        auto *search = window.findChild<QLineEdit *>("searchInput");
        auto *table = window.findChild<QTableView *>("resultsTable");
        QCOMPARE(table->model()->rowCount(), 257);
        search->setText("glass leed");
        QVERIFY(table->model()->rowCount() > 0);
        QVERIFY(window.findChild<QLabel *>("resultCount")->text().contains("Similar names"));
        window.findChild<QCheckBox *>("allowTypos")->setChecked(false);
        QCOMPARE(table->model()->rowCount(), 0);
        search->setText("glass lead");
        table->selectRow(1);
        const QPersistentModelIndex selected(table->model()->index(1, 0));
        window.refreshLibrary(); QTRY_VERIFY_WITH_TIMEOUT(!window.isScanning(), 5000);
        QVERIFY(selected.isValid());
        QCOMPARE(table->selectionModel()->selectedRows().first(), QModelIndex(selected));
        search->setText("hidden lead"); QCOMPARE(table->model()->rowCount(), 1);
        const QString preview = window.findChild<QPlainTextEdit *>("resultDetails")->toPlainText();
        QVERIFY(preview.startsWith("Embedded text\nHidden Lead"));
        QVERIFY(preview.size() < 5000);
        window.findChild<QCheckBox *>("namesOnly")->setChecked(true);
        QCOMPARE(table->model()->rowCount(), 0);
        window.findChild<QComboBox *>("sortChoice")->setCurrentIndex(2);
        window.refreshLibrary(); QTRY_VERIFY_WITH_TIMEOUT(!window.isScanning(), 5000);
        QVERIFY(window.findChild<QLabel *>("statusText")->text().contains("3 reused"));
        window.rebuildLibrary(); QTRY_VERIFY_WITH_TIMEOUT(!window.isScanning(), 5000);
        QVERIFY(window.findChild<QLabel *>("statusText")->text().contains("3 read"));
    }
    void readerCancellationAndNonRegularFiles() {
        std::atomic_bool cancel{true};
        QVERIFY(!BankReader::parse(fixture("generated/standard.fxb"), &cancel).isValid());
        QVERIFY(!BankReader::readFile(root_ + "/generated/standard.fxb", &cancel).isValid());
        QTemporaryDir dir;
        QVERIFY(!BankReader::readFile(dir.path()).isValid());
#ifdef Q_OS_UNIX
        const QString pipe = dir.filePath("pipe.fxb");
        QCOMPARE(::mkfifo(QFile::encodeName(pipe).constData(), 0600), 0);
        QVERIFY(BankReader::readFile(pipe).error.contains("regular file"));
#endif
    }
    void parserPresetLimit() {
        const auto preset = fixture("generated/parameter.fxp");
        auto bank = fixture("generated/standard.fxb").left(156);
        for (int i = 0; i <= BankReader::MaxPresets; ++i) bank += preset;
        qToBigEndian<quint32>(bank.size() - 8, reinterpret_cast<uchar *>(bank.data() + 4));
        qToBigEndian<quint32>(BankReader::MaxPresets + 1, reinterpret_cast<uchar *>(bank.data() + 24));
        const auto result = BankReader::parse(bank);
        QVERIFY(result.error.contains("preset limit"));
        QVERIFY(result.presets.isEmpty());
    }
    void seededParserMutations() {
        // Reproducible malformed headers, lengths, names and truncations. Run
        // the same corpus under ASan/UBSan in CI to catch invalid memory access.
        std::mt19937 random(0x465842);
        const QStringList files{"generated/standard.fxb", "generated/chunk.fxp", "generated/opaque.fxb"};
        for (const auto &name : files) {
            const auto original = fixture(name);
            for (int i = 0; i < 1000; ++i) {
                auto bytes = original;
                for (int j = 0; j < 1 + i % 8; ++j) bytes[int(random() % bytes.size())] = char(random() & 255);
                if (i % 3 == 0) bytes.truncate(int(random() % bytes.size()));
                const auto result = BankReader::parse(bytes);
                QVERIFY(result.embeddedText.size() <= BankReader::MaxTextCharacters);
                QVERIFY(result.presets.size() <= BankReader::MaxPresets);
                if (result.isValid()) QVERIFY(!result.format.isEmpty());
            }
        }
    }
    void discoveryAndNoticeBudgets() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        for (int i = 0; i < 20; ++i) write(dir.filePath(QString::number(i) + ".txt"), "x");
        std::atomic_bool cancel{false};
        ScanLimits limits; limits.directoryEntries = 3;
        auto result = scanFolders({dir.path()}, cancel, {}, limits);
        QVERIFY(result.issues.join(' ').contains("traversal limit"));
        QStringList missing;
        for (int i = 0; i < 400; ++i) missing += dir.filePath("missing" + QString::number(i));
        result = scanFolders(missing, cancel);
        QCOMPARE(result.issues.size(), 257);
        QVERIFY(result.issues.last().contains("omitted"));
    }
    void normalizedSnippetKeepsMatchedContext() {
        BankDocument bank; bank.format = "FBCh";
        bank.embeddedText = QString(500, ' ') + QString::fromUtf8("Étoile\n    Pad");
        const auto index = SearchIndex::build({{"/banks/test.fxb", bank}});
        const auto found = index->search("\"etoile pad\"");
        QCOMPARE(found.hits.size(), 1);
        QCOMPARE(found.hits[0].source, MatchSource::EmbeddedText);
        QVERIFY(index->snippet(found.hits[0]).contains("etoile pad"));
    }
    void preferencesPersistAndErrorsAreVisible() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto path = dir.filePath("preferences.ini");
        {
            MainWindow window(path);
            window.findChild<QCheckBox *>("namesOnly")->setChecked(true);
            window.findChild<QCheckBox *>("allowTypos")->setChecked(false);
            window.findChild<QComboBox *>("sortChoice")->setCurrentIndex(2);
        }
        MainWindow restored(path);
        QVERIFY(restored.findChild<QCheckBox *>("namesOnly")->isChecked());
        QVERIFY(!restored.findChild<QCheckBox *>("allowTypos")->isChecked());
        QCOMPARE(restored.findChild<QComboBox *>("sortChoice")->currentIndex(), 2);
        // A regular file cannot contain a settings file, on any OS or user.
        write(dir.filePath("blocker"), "x");
        MainWindow blocked(dir.filePath("blocker/prefs.ini"));
        blocked.clearLibrary();
        QVERIFY(!blocked.findChild<QLabel *>("settingsWarning")->isHidden());
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
