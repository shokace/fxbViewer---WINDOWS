#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fxb {
struct Preset {
    QString name;
    int number = 0; // One-based position in the bank.
};

struct BankDocument {
    QString format;
    QString pluginId;
    QVector<Preset> presets;
    QString embeddedText;
    QStringList warnings;
    QString error;
    bool isValid() const { return error.isEmpty(); }
};

class BankReader {
public:
    static constexpr qint64 MaxFileBytes = 64 * 1024 * 1024;
    static constexpr int MaxTextCharacters = 256 * 1024;
    static BankDocument readFile(const QString &path);
    static BankDocument parse(const QByteArray &bytes);
};
}
