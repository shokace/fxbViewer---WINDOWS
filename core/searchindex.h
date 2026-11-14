#pragma once
#include "library.h"
#include <QHash>
#include <memory>

namespace fxb {
enum class MatchSource { None, Name, SimilarName, Bank, Plugin, EmbeddedText, Path };
struct SearchHit {
    int row = -1;
    int score = 0;
    MatchSource source = MatchSource::None;
    QString matchedTerm;
};
struct SearchOptions { bool namesOnly = false; bool allowTypos = true; };
struct SearchStats {
    int candidatesExamined = 0;
    int embeddedTextChecks = 0;
    qint64 elapsedNanoseconds = 0;
    bool usedTypos = false;
};
struct SearchResults { QVector<SearchHit> hits; SearchStats stats; };

class SearchIndex {
public:
    struct Record { QString name, normalizedName; int bank = 0; int number = 0; };
    static std::shared_ptr<const SearchIndex> build(const QVector<LibraryEntry> &entries,
                                                   const std::atomic_bool *cancel = nullptr);
    static QString normalize(const QString &text);
    SearchResults search(const QString &query, SearchOptions options = {}) const;
    QString snippet(const SearchHit &hit) const;
    const Record &record(int row) const { return records_[row]; }
    const LibraryEntry &entry(int row) const { return entries_[records_[row].bank]; }
    int size() const { return records_.size(); }
    int bankCount() const { return entries_.size(); }
    const QVector<LibraryEntry> &entries() const { return entries_; }
private:
    struct BankFields { QString name, plugin, path, text; int begin = 0, end = 0; };
    using Postings = QHash<quint64, QVector<int>>;
    QVector<LibraryEntry> entries_;
    QVector<Record> records_;
    QVector<BankFields> banks_;
    Postings namePostings_, bankPostings_;
    QVector<int> fallbackBanks_;
    int bankPostingCount_ = 0;
    QHash<QString, int> wordIds_;
    QVector<QString> words_;
    QVector<QVector<int>> wordRows_;
    QHash<QString, QVector<int>> deletions_;
    static void addPostings(Postings &postings, const QString &text, int id);
    static QVector<int> lookup(const Postings &postings, const QString &term, int count);
    QVector<int> candidates(const QString &term, bool namesOnly) const;
    QVector<int> similarWords(const QString &term) const;
};
QString matchSourceLabel(MatchSource source);
}
