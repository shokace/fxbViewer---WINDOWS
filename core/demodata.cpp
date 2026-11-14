#include "demodata.h"
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QtEndian>

namespace fxb {
namespace {
void append32(QByteArray &b, quint32 n) {
    char bytes[4]; qToBigEndian<quint32>(n, reinterpret_cast<uchar *>(bytes)); b.append(bytes, 4);
}
QByteArray record(const char *kind, quint32 count, const QByteArray &payload) {
    QByteArray b("CcnK", 4); append32(b, payload.size() + 20);
    b.append(kind, 4); append32(b, 1); b.append("Demo", 4); append32(b, 1); append32(b, count); b += payload;
    return b;
}
bool save(const QString &path, const QByteArray &data, QString *error) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly) || f.write(data) != data.size()) {
        if (error) *error = path + ": " + f.errorString();
        return false;
    }
    return true;
}
}
bool createDemoLibrary(const QString &directory, int banks, int chunkBanks, QString *error) {
    if (!QDir().mkpath(directory)) { if (error) *error = "Could not create demo directory."; return false; }
    const QStringList collections{"Aurora Keys", "Night Drive", "Analog Sketches", "Cinematic Air"};
    const QStringList names{"Glass Lead", "Velvet Pad", "Deep Bass", "Crystal Keys", "Silver Bell", "Warm Brass",
        "Plucked Moon", "Amber Strings", "Dusty Piano", "Wide Choir", "Étoile Pad", "Sub Bass",
        "Glass Lead Wide", "Lead Glass Harmonics", "Neon Arp", "Ocean Drone", "Tape Flute", "Soft Organ",
        "Chrome Pluck", "Violet Sequence", "Hollow Marimba", "Golden Horn", "Dream Chimes", "Electric Reed"};
    for (int bank = 0; bank < banks; ++bank) {
        const QString collection = collections[bank % collections.size()];
        QDir().mkpath(directory + '/' + collection);
        QByteArray payload(128, '\0');
        for (int preset = 0; preset < 128; ++preset) {
            QString name = names[(bank * 7 + preset) % names.size()];
            if (preset >= 24) name += QString(" %1").arg(preset / 24 + 1);
            QByteArray program = name.toUtf8().left(27); program.append(28 - program.size(), '\0');
            append32(program, 0x3e800000); append32(program, 0x3f400000);
            payload += record("FxCk", 2, program);
        }
        const QString path = directory + '/' + collection + '/' + collection + QString(" %1.fxb").arg(bank + 1, 3, 10, QLatin1Char('0'));
        if (!save(path, record("FxBk", 128, payload), error)) return false;
    }
    QDir().mkpath(directory + "/Legacy Chunks");
    for (int bank = 0; bank < chunkBanks; ++bank) {
        QByteArray text("Hidden Lead"); text.append('\0'); text += "Vintage Chorale"; text.append('\0');
        // Representative readable private state, separate from preset names.
        const QByteArray metadata("oscillator=0.75; filter=0.2; envelope=0.9; routing=serial;\n");
        while (text.size() < 128 * 1024) text += metadata;
        QByteArray payload(128, '\0'); append32(payload, text.size()); payload += text;
        if (!save(directory + QString("/Legacy Chunks/Archive %1.fxb").arg(bank + 1, 3, 10, QLatin1Char('0')),
                  record("FBCh", 128, payload), error)) return false;
    }
    return true;
}
}
