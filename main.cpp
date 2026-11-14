#include "mainwindow.h"
#include "core/demodata.h"
#include "core/resultmodel.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
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
    QCoreApplication::setApplicationVersion("2.1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Find remembered presets in FXB and FXP sound banks.");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"demo", "Open an isolated library of synthetic sample banks."});
    parser.addOption({"demo-capture", "Save a four-search screenshot tour (requires --demo).", "directory"});
    parser.process(app);
    const bool demoMode = parser.isSet("demo");
    const QString capture = parser.value("demo-capture");
    if ((!capture.isEmpty() && !demoMode) || (!capture.isEmpty() && !QDir().mkpath(capture))) {
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
            if (!window.grab().save(path)) { qCritical("Could not save %s", qPrintable(path)); tour.stop(); return; }
            auto *model = qobject_cast<fxb::SearchModel *>(window.findChild<QTableView *>("resultsTable")->model());
            captures += QJsonObject{{"query", queries[step]}, {"results", model->rowCount()},
                {"search_ms", model->stats().elapsedNanoseconds / 1e6}, {"typo_recovery", model->stats().usedTypos}, {"screenshot", names[step]}};
            if (++step < queries.size()) window.findChild<QLineEdit *>("searchInput")->setText(queries[step]);
            else {
                tour.stop();
                QFile report(QDir(capture).filePath("tour.json"));
                if (report.open(QIODevice::WriteOnly)) report.write(QJsonDocument(captures).toJson());
                window.findChild<QLineEdit *>("searchInput")->setText("glass leed");
            }
        });
        QTimer::singleShot(0, &window, [&] { window.addFolder(demo->path() + "/Sample Library"); });
    }
    window.show();
    return app.exec();
}
