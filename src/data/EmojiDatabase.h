/**
 * EmojiDatabase - Loads and manages Unicode emoji data
 */
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVector>
#include <QtCore/QVariantMap>

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

    // Total count
    int count() const { return m_emojis.size(); }

signals:
    void loaded();
    void loadFailed(const QString& error);

private:
    QVector<EmojiEntry> m_emojis;
    QStringList m_categories;
    QMap<QString, QVector<int>> m_categoryIndex;   // category -> indices
    QMap<QString, QVector<int>> m_keywordIndex;    // keyword -> indices
    QHash<QString, int> m_codepointsIndex;         // codepoints -> index
    QHash<QString, int> m_charIndex;               // emojiChar -> index

    // Parse emoji-test.txt format
    bool parseEmojiTest(const QString& data);

    // Parse emoji-zwj-sequences.txt
    bool parseZwjSequences(const QString& data);

    // Build search indexes
    void buildIndexes();

    // Attach purpose keywords
    void attachPurposeKeywords(EmojiEntry& entry);

    // Generate emoji character from codepoints
    QString codepointsToChar(const QString& codepoints) const;
};