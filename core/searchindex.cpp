#include "searchindex.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <numeric>

namespace fxb {
namespace {
quint64 gram(const QString &s, int i) {
    return (quint64(s[i].unicode()) << 32) | (quint64(s[i+1].unicode()) << 16) | s[i+2].unicode();
}
QVector<int> intersect(const QVector<int> &a, const QVector<int> &b) {
    QVector<int> out; out.reserve(std::min(a.size(), b.size()));
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out)); return out;
}
QVector<int> unite(const QVector<int> &a, const QVector<int> &b) {
    QVector<int> out; out.reserve(a.size() + b.size());
    std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out)); return out;
}
QVector<int> allRows(int count) { QVector<int> out(count); std::iota(out.begin(), out.end(), 0); return out; }
bool oneEdit(const QString &a, const QString &b) {
    if (qAbs(a.size() - b.size()) > 1) return false;
    if (a.size() == b.size()) {
        QVector<int> different;
        for (int i = 0; i < a.size(); ++i) if (a[i] != b[i]) { different += i; if (different.size() > 2) return false; }
        return different.size() <= 1 || (different[1] == different[0] + 1
            && a[different[0]] == b[different[1]] && a[different[1]] == b[different[0]]);
    }
    const QString &shorter = a.size() < b.size() ? a : b;
    const QString &longer = a.size() < b.size() ? b : a;
    int i = 0, j = 0, skips = 0;
    while (i < shorter.size() && j < longer.size()) {
        if (shorter[i] == longer[j]) { ++i; ++j; }
        else { ++j; if (++skips > 1) return false; }
    }
    return true;
}
QStringList termsFor(const QString &query, bool &quoted) {
    QStringList terms;
    int pos = 0;
    auto isQuote = [](QChar c) { return c == '"' || c == QChar(0x201c) || c == QChar(0x201d); };
    while (pos < query.size()) {
        if (query[pos].isSpace()) { ++pos; continue; }
        if (isQuote(query[pos])) {
            quoted = true;
            const int start = ++pos;
            while (pos < query.size() && !isQuote(query[pos])) ++pos;
            const QString term = SearchIndex::normalize(query.mid(start, pos - start));
            if (!term.isEmpty()) terms += term;
            if (pos < query.size()) ++pos;
        } else {
            const int start = pos;
            while (pos < query.size() && !query[pos].isSpace() && !isQuote(query[pos])) ++pos;
            terms += SearchIndex::normalize(query.mid(start, pos - start)).split(' ', Qt::SkipEmptyParts);
        }
    }
    terms.removeDuplicates();
    return terms;
}
struct FieldMatch { int score = 0; MatchSource source = MatchSource::None; };
}

QString SearchIndex::normalize(const QString &text) {
    const QString decomposed = text.toCaseFolded().normalized(QString::NormalizationForm_D);
    QString out; out.reserve(decomposed.size());
    bool space = true;
    for (const QChar c : decomposed) {
        if (c.category() == QChar::Mark_NonSpacing || c.category() == QChar::Mark_SpacingCombining || c.category() == QChar::Mark_Enclosing) continue;
        const bool separator = c.isSpace() || c == '_' || c == '-' || c == '/' || c == '\\' || c == '.';
        if (separator) { if (!space) out += ' '; space = true; }
        else { out += c; space = false; }
    }
    if (out.endsWith(' ')) out.chop(1);
    return out;
}

void SearchIndex::addPostings(Postings &postings, const QString &text, int id) {
    QSet<quint64> unique;
    for (int i = 0; i + 2 < text.size(); ++i) unique.insert(gram(text, i));
    for (quint64 key : unique) postings[key].append(id);
}

std::shared_ptr<const SearchIndex> SearchIndex::build(const QVector<LibraryEntry> &entries, const std::atomic_bool *cancel) {
    auto index = std::make_shared<SearchIndex>();
    for (const auto &entry : entries) {
        if (cancel && cancel->load()) return {};
        if (!entry.bank.isValid()) continue;
        const int bankId = index->entries_.size();
        index->entries_ += entry;
        BankFields fields{normalize(QFileInfo(entry.path).fileName()), normalize(entry.bank.pluginId),
            normalize(entry.path), normalize(entry.bank.embeddedText), int(index->records_.size()), 0};
        const QString searchable = fields.name + '\n' + fields.plugin + '\n' + fields.path + '\n' + fields.text;
        QSet<quint64> bankGrams;
        // High-entropy private chunks should not explode the posting tables.
        // Fall back to exact verification for these banks, never drop matches.
        constexpr int perBankLimit = 8192, totalBankPostings = 2000000;
        for (int i = 0; i + 2 < searchable.size(); ++i) {
            if (i % 4096 == 0 && cancel && cancel->load()) return {};
            bankGrams.insert(gram(searchable, i));
            if (bankGrams.size() > perBankLimit || index->bankPostingCount_ + bankGrams.size() > totalBankPostings) break;
        }
        if (bankGrams.size() > perBankLimit || index->bankPostingCount_ + bankGrams.size() > totalBankPostings)
            index->fallbackBanks_ += bankId;
        else {
            for (quint64 key : bankGrams) index->bankPostings_[key].append(bankId);
            index->bankPostingCount_ += bankGrams.size();
        }
        auto add = [&](const QString &name, int number) {
            const int row = index->records_.size();
            const QString normalized = normalize(name);
            index->records_.push_back({name, normalized, bankId, number});
            addPostings(index->namePostings_, normalized, row);
            const QStringList tokens = normalized.split(' ', Qt::SkipEmptyParts);
            for (const QString &word : tokens) {
                if (word.size() < 4 || word.size() > 32) continue;
                bool alphabetic = false;
                for (QChar c : word) if (c.isLetter()) { alphabetic = true; break; }
                if (!alphabetic) continue;
                auto found = index->wordIds_.constFind(word);
                int id;
                if (found == index->wordIds_.constEnd()) {
                    // Exact search remains complete; cap only the typo vocabulary.
                    if (index->words_.size() >= 20000) continue;
                    id = index->words_.size(); index->words_ += word; index->wordRows_.append(QVector<int>{});
                    index->wordIds_.insert(word, id);
                    QSet<QString> keys{word};
                    for (int i = 0; i < word.size(); ++i) { QString key = word; key.remove(i, 1); keys.insert(key); }
                    for (const auto &key : keys) index->deletions_[key].append(id);
                } else id = found.value();
                auto &rows = index->wordRows_[id];
                if (rows.isEmpty() || rows.last() != row) rows += row;
            }
        };
        if (entry.bank.presets.isEmpty()) add({}, 0);
        else for (const auto &preset : entry.bank.presets) {
            if (cancel && cancel->load()) return {};
            add(preset.name, preset.number);
        }
        fields.end = index->records_.size();
        index->banks_ += fields;
    }
    return index;
}

QVector<int> SearchIndex::lookup(const Postings &postings, const QString &term, int count) {
    if (term.size() < 3) return allRows(count);
    QVector<const QVector<int> *> lists;
    QSet<quint64> seen;
    for (int i = 0; i + 2 < term.size(); ++i) {
        const auto key = gram(term, i);
        if (seen.contains(key)) continue;
        seen.insert(key);
        auto it = postings.constFind(key);
        if (it == postings.constEnd()) return {};
        lists += &it.value();
    }
    std::sort(lists.begin(), lists.end(), [](const auto *a, const auto *b) { return a->size() < b->size(); });
    QVector<int> rows = *lists[0];
    for (int i = 1; i < lists.size() && !rows.isEmpty(); ++i) rows = intersect(rows, *lists[i]);
    return rows;
}

QVector<int> SearchIndex::candidates(const QString &term, bool namesOnly) const {
    auto rows = lookup(namePostings_, term, records_.size());
    if (namesOnly) return rows;
    const auto banks = unite(lookup(bankPostings_, term, banks_.size()), fallbackBanks_);
    QVector<int> bankRows;
    for (int bank : banks) for (int row = banks_[bank].begin; row < banks_[bank].end; ++row) bankRows += row;
    return unite(rows, bankRows);
}

QVector<int> SearchIndex::similarWords(const QString &term) const {
    if (term.size() < 4 || term.size() > 32 || term.contains(' ')) return {};
    QSet<int> ids;
    auto collect = [&](const QString &key) {
        const auto it = deletions_.constFind(key);
        if (it != deletions_.constEnd()) for (int id : it.value()) ids.insert(id);
    };
    collect(term);
    for (int i = 0; i < term.size(); ++i) { QString key = term; key.remove(i, 1); collect(key); }
    QVector<int> out;
    for (int id : ids) if (words_[id] != term && oneEdit(term, words_[id])) out += id;
    std::sort(out.begin(), out.end(), [&](int a, int b) { return words_[a] < words_[b]; });
    return out;
}

SearchResults SearchIndex::search(const QString &rawQuery, SearchOptions options) const {
    QElapsedTimer timer; timer.start();
    SearchResults result;
    const QString query = rawQuery.trimmed();
    if (query.size() > 256) { result.stats.elapsedNanoseconds = timer.nsecsElapsed(); return result; }
    bool quoted = false;
    const QStringList terms = termsFor(query, quoted);
    if (query.isEmpty()) {
        for (int row = 0; row < records_.size(); ++row)
            if (!options.namesOnly || records_[row].number > 0) result.hits += {row, 0, MatchSource::None, {}};
        result.stats.elapsedNanoseconds = timer.nsecsElapsed(); return result;
    }
    if (terms.isEmpty() || query.size() > 256) { result.stats.elapsedNanoseconds = timer.nsecsElapsed(); return result; }
    auto perform = [&](bool fuzzy) {
        QVector<QVector<int>> alternatives;
        QVector<QVector<int>> termCandidates;
        for (const auto &term : terms) {
            auto rows = candidates(term, options.namesOnly);
            auto ids = fuzzy ? similarWords(term) : QVector<int>{};
            for (int id : ids) rows = unite(rows, wordRows_[id]);
            alternatives += ids; termCandidates += rows;
        }
        std::sort(termCandidates.begin(), termCandidates.end(), [](const auto &a, const auto &b) { return a.size() < b.size(); });
        auto possible = termCandidates.first();
        for (int i = 1; i < termCandidates.size() && !possible.isEmpty(); ++i) possible = intersect(possible, termCandidates[i]);
        // Check large private chunks once per bank/term, not once per preset.
        QVector<QHash<int, FieldMatch>> bankMatches(terms.size());
        for (int row : possible) {
            ++result.stats.candidatesExamined;
            const auto &record = records_[row];
            const auto &bank = banks_[record.bank];
            SearchHit hit{row, 0, MatchSource::Name, {}};
            int weakest = 100000;
            bool accepted = true;
            for (int t = 0; t < terms.size(); ++t) {
                const QString &term = terms[t];
                FieldMatch match;
                if (record.normalizedName.contains(term)) {
                    const bool wholeWord = (" " + record.normalizedName + " ").contains(" " + term + " ");
                    match = {wholeWord ? 1500 : 1000, MatchSource::Name};
                }
                if (!options.namesOnly && match.score == 0) {
                    auto it = bankMatches[t].constFind(record.bank);
                    if (it == bankMatches[t].constEnd()) {
                        FieldMatch bm;
                        if (bank.name.contains(term)) bm = {400, MatchSource::Bank};
                        else if (bank.plugin.contains(term)) bm = {350, MatchSource::Plugin};
                        else {
                            if (!bank.text.isEmpty()) {
                                ++result.stats.embeddedTextChecks;
                                if (bank.text.contains(term)) bm = {200, MatchSource::EmbeddedText};
                            }
                            if (!bm.score && bank.path.contains(term)) bm = {100, MatchSource::Path};
                        }
                        it = bankMatches[t].insert(record.bank, bm);
                    }
                    match = it.value();
                }
                if (fuzzy && match.score < 700) for (int id : alternatives[t]) {
                    if (std::binary_search(wordRows_[id].begin(), wordRows_[id].end(), row)) {
                        match = {700, MatchSource::SimilarName}; break;
                    }
                }
                if (!match.score) { accepted = false; break; }
                hit.score += match.score;
                if (match.score < weakest) { weakest = match.score; hit.source = match.source; hit.matchedTerm = term; }
            }
            if (!accepted) continue;
            const QString phrase = terms.join(' ');
            if (record.normalizedName == phrase) hit.score += 4000;
            else if (record.normalizedName.startsWith(phrase)) hit.score += 1000;
            else if (record.normalizedName.contains(phrase)) hit.score += 500;
            result.hits += hit;
        }
    };
    perform(false);
    if (result.hits.isEmpty() && options.allowTypos && !quoted) {
        perform(true);
        result.stats.usedTypos = !result.hits.isEmpty();
    }
    result.stats.elapsedNanoseconds = timer.nsecsElapsed();
    return result;
}

QString SearchIndex::snippet(const SearchHit &hit) const {
    if (hit.row < 0 || hit.row >= records_.size()) return {};
    const auto &r = records_[hit.row];
    const auto &e = entries_[r.bank];
    if (hit.source == MatchSource::EmbeddedText) {
        // Offsets in normalized text differ after accent/whitespace folding.
        // Build context from the same normalized field used by the matcher,
        // including phrases spanning lines, so the match cannot disappear.
        const QString &original = e.bank.embeddedText;
        const int originalPos = original.indexOf(hit.matchedTerm, 0, Qt::CaseInsensitive);
        const QString &text = originalPos >= 0 ? original : banks_[r.bank].text;
        const int pos = originalPos >= 0 ? originalPos : text.indexOf(hit.matchedTerm);
        if (pos >= 0) {
            const int start = std::max(0, pos - 48);
            return (start ? QStringLiteral("…") : QString()) + text.mid(start, 180)
                + (text.size() > start + 180 ? QStringLiteral("…") : QString());
        }
    }
    if (hit.source == MatchSource::Bank) return QFileInfo(e.path).fileName();
    if (hit.source == MatchSource::Plugin) return e.bank.pluginId;
    if (hit.source == MatchSource::Path) return e.path;
    return r.name;
}

QString matchSourceLabel(MatchSource source) {
    switch (source) {
    case MatchSource::Name: return QStringLiteral("Preset name");
    case MatchSource::SimilarName: return QStringLiteral("Similar name");
    case MatchSource::Bank: return QStringLiteral("Bank name");
    case MatchSource::Plugin: return QStringLiteral("Plugin ID");
    case MatchSource::EmbeddedText: return QStringLiteral("Embedded text");
    case MatchSource::Path: return QStringLiteral("Folder path");
    default: return {};
    }
}
}
