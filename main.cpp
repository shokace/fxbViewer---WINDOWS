#include "mainwindow.h"
#include "core/demodata.h"
#include "core/resultmodel.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("fxbViewer");
    QCoreApplication::setApplicationName("FXB Viewer");
    QCoreApplication::setApplicationVersion(QStringLiteral(FXBVIEWER_VERSION));
    QCommandLineParser parser;
    parser.setApplicationDescription("Find remembered presets in FXB and FXP sound banks.");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"demo", "Open an isolated library of synthetic sample banks."});
    parser.addOption({"demo-capture", "Save a four-search screenshot tour (requires --demo).", "directory"});
    parser.addOption({"smoke-test", "Run an isolated four-search UI check, save evidence, and exit.", "directory"});
    parser.process(app);
    const bool smoke = parser.isSet("smoke-test");
    const bool demoMode = parser.isSet("demo") || smoke;
    const QString capture = parser.value(smoke ? "smoke-test" : "demo-capture");
    if (((smoke || parser.isSet("demo-capture")) && capture.isEmpty())
        || (smoke && parser.isSet("demo-capture")) || (!capture.isEmpty() && !demoMode) || (!capture.isEmpty() && !QDir().mkpath(capture))) {
        qCritical("--demo-capture requires --demo and a writable directory."); return 2;
    }
    std::unique_ptr<QTemporaryDir> demo;
    QString settings;
    if (demoMode) {
        demo = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/fxbviewer-demo-XXXXXX");
        QString error;
        if (!demo->isValid() || !fxb::createDemoLibrary(demo->path() + "/Sample Library", 192, 24, &error)) {
            qCritical("Could not create demo: %s", qPrintable(error)); return 1;
        }
        settings = demo->path() + "/demo-settings.ini";
    }
    MainWindow window(settings, nullptr, demoMode);
    bool loaded = false;
    int step = 0;
    QJsonArray captures;
    const QStringList queries{"glass lead", "lead glass", "glass leed", "hidden lead"};
    const QStringList names{"01-name-search.png", "02-word-order.png", "03-spelling-recovery.png", "04-embedded-text.png"};
    QTimer tour;
    if (demoMode) {
        QObject::connect(&window, &MainWindow::libraryReady, &window, [&] {
            if (loaded) return;
            loaded = true;
            window.findChild<QLineEdit *>("searchInput")->setText(queries[0]);
            if (!capture.isEmpty()) tour.start(350);
        });
        QObject::connect(&tour, &QTimer::timeout, &window, [&] {
            const QString path = QDir(capture).filePath(names[step]);
            if (!window.grab().save(path)) { qCritical("Could not save %s", qPrintable(path)); tour.stop(); app.exit(3); return; }
            auto *model = qobject_cast<fxb::SearchModel *>(window.findChild<QTableView *>("resultsTable")->model());
            const int expected[] = {3072, 3072, 3072, 24};
            if (smoke && (model->rowCount() != expected[step] || model->stats().usedTypos != (step == 2))) {
                qCritical("Smoke test failed at query %d", step); tour.stop(); app.exit(4); return;
            }
            captures += QJsonObject{{"query", queries[step]}, {"results", model->rowCount()},
                {"search_ms", model->stats().elapsedNanoseconds / 1e6}, {"typo_recovery", model->stats().usedTypos}, {"screenshot", names[step]}};
            if (++step < queries.size()) window.findChild<QLineEdit *>("searchInput")->setText(queries[step]);
            else {
                tour.stop();
                QSaveFile report(QDir(capture).filePath("tour.json"));
                const auto json = QJsonDocument(captures).toJson();
                if (!report.open(QIODevice::WriteOnly) || report.write(json) != json.size() || !report.commit()) {
                    qCritical("Could not write tour.json"); app.exit(3); return;
                }
                if (smoke) { app.exit(0); return; }
                window.findChild<QLineEdit *>("searchInput")->setText("glass leed");
            }
        });
        QTimer::singleShot(0, &window, [&] { window.addFolder(demo->path() + "/Sample Library"); });
    }
    if (smoke) QTimer::singleShot(30000, &app, [&] { qCritical("Smoke test timed out"); app.exit(5); });
    window.show();
    return app.exec();
}
