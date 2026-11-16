/**
 * EmojiDatabase - Loads and manages Unicode emoji data
 */
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVector>
#include <QtCore/QVariantMap>
#include <QtCore/QHash>
#include <QtCore/QStringView>

struct EmojiEntry
{
    QString codepoints;      // Space-separated hex codepoints (e.g., "1F600" or "1F468 200D 1F469 200D 1F467 200D 1F466")
    QString name;            // CLDR short name (e.g., "grinning face")
    QStringList keywords;    // Search keywords
    QString category;        // Category name
    QString subcategory;     // Subcategory
    QStringList skinTones;   // Available skin tone variants (codepoints)
    bool hasSkinTones = false;
    bool isZwjSequence = false;
    QString emojiChar;       // Actual emoji character(s) for display
    bool isSkinToneVariant = false; // true if this entry has Fitzpatrick modifier (1F3FB..1F3FF)
    int toneIndex = 0;              // 0: default/yellow, 1: 1F3FB, 2: 1F3FC, 3: 1F3FD, 4: 1F3FE, 5: 1F3FF
    int baseEmojiIndex = -1;        // Index of the base emoji if this is a variant
    int skinToneVariantsByTone[6] = {-1, -1, -1, -1, -1, -1}; // [0]=base, [1..5]=variant indices
};

class EmojiDatabase : public QObject
{
    Q_OBJECT
public:
    explicit EmojiDatabase(QObject* parent = nullptr);
    ~EmojiDatabase() override = default;

    // Load emoji data from embedded resources or files
    bool load();

    // Get all emojis
    const QVector<EmojiEntry>& allEmojis() const { return m_emojis; }

    // Get emojis by category
    QVector<EmojiEntry> emojisByCategory(const QString& category) const;

    // Get unique categories
    QStringList categories() const { return m_categories; }

    // Search emojis by keyword/name (returns entries sorted by relevance)
    QVector<EmojiEntry> search(const QString& query) const;

    // Search emojis returning indices directly (sorted by relevance)
    QVector<int> searchIndices(const QString& query) const;

    // Fast Damerau-Levenshtein distance calculation (returns maxDist + 1 if exceeded)
    static int damerauLevenshtein(QStringView s1, QStringView s2, int maxDist);

    // Calculate relevance match score between query and emoji entry
    static int calculateMatchScore(const QString& query, const QString& name, const QStringList& keywords, bool hasSkinTones = false, const QString& category = QString());

    // Get emoji by codepoints (e.g. "1F600")
    const EmojiEntry* findByCodepoints(const QString& codepoints) const;

    // Get emoji by character (e.g. "😀")
    const EmojiEntry* findByChar(const QString& emojiChar) const;

    // Get emoji by either character or codepoints
    const EmojiEntry* findEmoji(const QString& codeOrChar) const;

    // Get recent emojis (from settings codes or chars)
    QVector<EmojiEntry> recentEmojis(const QStringList& recentCodes) const;

    // Get frequent emojis (from settings)
    QVector<EmojiEntry> frequentEmojis(const QVariantMap& frequentCodes) const;

    // Get skin tone variants for an emoji
    QVector<EmojiEntry> skinToneVariants(const EmojiEntry& base) const;

    // Get specific skin tone variant (0 = default/yellow, 1..5 = Light..Dark)
    const EmojiEntry* variantForTone(const EmojiEntry& base, int tone) const;

    // Total count
    int count() const { return m_emojis.size(); }

signals:
    void loaded();
    void loadFailed(const QString& error);

private:
    struct WordPosting {
        int emojiIndex;
        enum Role : uint8_t {
            ExactName = 0,
            FirstNameWord = 1,
            PrimaryKeyword = 2,
            NameWord = 3,
            Keyword = 4
        } role;
    };

    struct VocabEntry {
        QString word;
        QVector<WordPosting> postings;
    };

    QVector<EmojiEntry> m_emojis;
    QStringList m_categories;
    QMap<QString, QVector<int>> m_categoryIndex;   // category -> indices
    QMap<QString, QVector<int>> m_keywordIndex;    // keyword -> indices
    QHash<QString, int> m_codepointsIndex;         // codepoints -> index
    QHash<QString, int> m_charIndex;               // emojiChar -> index

    QVector<VocabEntry> m_vocabList;
    mutable QHash<QString, QVector<int>> m_searchCache;

    // Search single term returning mapping of emojiIndex -> relevance score
    QHash<int, int> searchSingleTerm(const QString& term) const;

    // Parse emoji-test.txt format
    bool parseEmojiTest(const QString& data);

    // Parse emoji-zwj-sequences.txt
    bool parseZwjSequences(const QString& data);

    // Group skin tone variants under their base emojis
    void groupSkinToneVariants();

    // Build search indexes
    void buildIndexes();

    // Attach purpose keywords
    void attachPurposeKeywords(EmojiEntry& entry);

    // Load universal kaomoji collection
    void loadKaomoji();

    // Load Unicode symbols, math, currencies, and technical characters
    void loadSymbols();

    // Generate emoji character from codepoints
    QString codepointsToChar(const QString& codepoints) const;
};
