#include "bankreader.h"

#include <QFile>
#include <QtEndian>

namespace fxb {
namespace {
quint32 u32(const QByteArray &b, int offset)
{
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(b.constData() + offset));
}

QString decode(const QByteArray &b)
{
    const QString utf8 = QString::fromUtf8(b);
    return utf8.contains(QChar::ReplacementCharacter) ? QString::fromLatin1(b) : utf8;
}

QString nameAt(const QByteArray &b, int offset)
{
    QByteArray name = b.mid(offset, 28);
    const int nul = name.indexOf('\0');
    if (nul >= 0) name.truncate(nul);
    QString result = decode(name).trimmed();
    for (QChar &ch : result) if (!ch.isPrint()) ch = QLatin1Char(' ');
    return result.trimmed();
}

QString extractText(const QByteArray &bytes, bool &limited)
{
    QString text;
    auto append = [&](const QByteArray &run) {
        if (run.size() < 2) return;
        const QString decoded = decode(run);
        const int available = BankReader::MaxTextCharacters - text.size();
        if (decoded.size() + 1 > available) limited = true;
        if (available > 0) text += (decoded + QLatin1Char('\n')).left(available);
    };
    QByteArray run;
    for (unsigned char ch : bytes) {
        if (ch >= 32 && ch != 127) run += char(ch);
        else { append(run); run.clear(); }
        if (run.size() > BankReader::MaxTextCharacters * 4) { append(run); run.clear(); }
        if (limited) return text;
    }
    append(run);
    // Some plugin chunks (including the Massive reference bank) store UTF-16
    // ASCII names. Try both byte orders and alignments, without interpreting
    // arbitrary binary data as structured presets.
    for (int alignment = 0; alignment < 2 && !limited; ++alignment) {
        for (int order = 0; order < 2 && !limited; ++order) {
            run.clear();
            for (int i = alignment; i + 1 < bytes.size(); i += 2) {
                const auto ch = static_cast<unsigned char>(bytes[i + order]);
                if (bytes[i + 1 - order] == 0 && ch >= 32 && ch < 127) run += char(ch);
                else { append(run); run.clear(); }
                if (run.size() > BankReader::MaxTextCharacters * 4) { append(run); run.clear(); }
                if (limited) break;
            }
            append(run);
        }
    }
    return text;
}

bool header(const QByteArray &b, int start, int end, BankDocument &doc)
{
    if (end - start < 28 || b.mid(start, 4) != "CcnK") {
        doc.error = QStringLiteral("Missing or truncated VST2 header (CcnK).");
        return false;
    }
    if (u32(b, start + 12) > 2) {
        doc.error = QStringLiteral("Unsupported VST2 format version.");
        return false;
    }
    return true;
}

bool sizeMatches(const QByteArray &b, int start, int expectedEnd, BankDocument &doc)
{
    const quint32 size = u32(b, start + 4);
    if (size == 0) {
        const QString warning = QStringLiteral("Outer size is unspecified; validated using payload lengths.");
        if (!doc.warnings.contains(warning)) doc.warnings += warning;
        return true;
    }
    if (quint64(size) + 8 != quint64(expectedEnd - start)) {
        doc.error = QStringLiteral("Declared VST2 size does not match the payload.");
        return false;
    }
    return true;
}

bool program(const QByteArray &b, int start, int limit, int number, int &end,
             BankDocument &doc, bool extractChunk)
{
    if (!header(b, start, limit, doc)) return false;
    const QByteArray magic = b.mid(start + 8, 4);
    if (limit - start < 56 || (magic != "FxCk" && magic != "FPCh")) {
        doc.error = QStringLiteral("Invalid or truncated program record.");
        return false;
    }
    quint64 length = 56;
    if (magic == "FxCk") length += quint64(u32(b, start + 24)) * 4;
    else {
        if (limit - start < 60) { doc.error = QStringLiteral("Missing program chunk length."); return false; }
        length = quint64(60) + u32(b, start + 56);
    }
    if (length > quint64(limit - start)) {
        doc.error = QStringLiteral("Program payload is truncated or has an invalid parameter count.");
        return false;
    }
    end = start + int(length);
    if (!sizeMatches(b, start, end, doc)) return false;
    doc.presets.push_back({nameAt(b, start + 28), number});
    if (magic == "FPCh" && extractChunk) {
        bool limited = false;
        doc.embeddedText = extractText(b.mid(start + 60, int(length) - 60), limited);
        if (limited) doc.warnings += QStringLiteral("Embedded text limit reached; some text was not indexed.");
    }
    return true;
}
}

BankDocument BankReader::readFile(const QString &path)
{
    QFile file(path);
    BankDocument doc;
    if (!file.open(QIODevice::ReadOnly)) { doc.error = file.errorString(); return doc; }
    if (file.size() > MaxFileBytes) { doc.error = QStringLiteral("File exceeds the 64 MiB size limit."); return doc; }
    const QByteArray bytes = file.read(MaxFileBytes + 1);
    if (file.error() != QFileDevice::NoError) { doc.error = file.errorString(); return doc; }
    return parse(bytes);
}

BankDocument BankReader::parse(const QByteArray &bytes)
{
    BankDocument doc;
    if (bytes.size() > MaxFileBytes) { doc.error = QStringLiteral("File exceeds the 64 MiB size limit."); return doc; }
    const int total = bytes.size();
    if (!header(bytes, 0, total, doc)) return doc;
    const QByteArray magic = bytes.mid(8, 4);
    doc.format = QString::fromLatin1(magic);
    doc.pluginId = QString::number(u32(bytes, 16), 16).rightJustified(8, QLatin1Char('0'));
    const QByteArray id = bytes.mid(16, 4);
    bool printable = true;
    for (unsigned char ch : id) if (ch < 32 || ch > 126) printable = false;
    if (printable) doc.pluginId = QString::fromLatin1(id);

    int end = 0;
    if (magic == "FxCk" || magic == "FPCh") {
        if (!program(bytes, 0, total, 1, end, doc, true)) return doc;
    } else if (magic == "FxBk") {
        if (total < 156) { doc.error = QStringLiteral("Bank header is truncated."); return doc; }
        const quint32 count = u32(bytes, 24);
        if (count > quint32((total - 156) / 56)) {
            doc.error = QStringLiteral("Bank program count exceeds the available data."); return doc;
        }
        end = 156;
        for (quint32 i = 0; i < count; ++i) {
            if (!program(bytes, end, total, int(i + 1), end, doc, false)) return doc;
        }
    } else if (magic == "FBCh") {
        if (total < 160 || quint64(u32(bytes, 156)) + 160 != quint64(total)) {
            doc.error = QStringLiteral("Bank chunk length is invalid or truncated."); return doc;
        }
        end = total;
        bool limited = false;
        doc.embeddedText = extractText(bytes.mid(160), limited);
        doc.warnings += QStringLiteral("Plugin-specific bank: searching embedded text; preset names are not decoded.");
        if (limited) doc.warnings += QStringLiteral("Embedded text limit reached; some text was not indexed.");
    } else {
        doc.error = QStringLiteral("Unsupported VST2 record type: %1.").arg(doc.format); return doc;
    }
    if (end != total) doc.error = QStringLiteral("Unexpected trailing data after the VST2 payload.");
    else sizeMatches(bytes, 0, total, doc);
    return doc;
}
}
