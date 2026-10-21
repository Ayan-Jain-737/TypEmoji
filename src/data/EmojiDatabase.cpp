/**
 * EmojiDatabase implementation - Loads Unicode emoji data
 */
#include "EmojiDatabase.h"
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QRegularExpression>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QCoreApplication>
#include <algorithm>

EmojiDatabase::EmojiDatabase(QObject* parent) : QObject(parent) {}

bool EmojiDatabase::load()
{
    // Try to load from resources first (embedded), then from files
    QString testPath = ":/emoji/emoji-test.txt";
    QString zwjPath = ":/emoji/emoji-zwj-sequences.txt";

    QFile testFile(testPath);
    QFile zwjFile(zwjPath);

    // If not in resources, check standard Linux search paths
    if (!testFile.exists()) {
        const QStringList candidateDirs = {
            QCoreApplication::applicationDirPath() + "/data",
            QCoreApplication::applicationDirPath() + "/../share/emojityper/data",
            "/usr/share/emojityper/data",
            "/usr/local/share/emojityper/data",
            QDir::currentPath() + "/data"
        };
        for (const QString& dir : candidateDirs) {
            if (QFile::exists(dir + "/emoji-test.txt")) {
                testFile.setFileName(dir + "/emoji-test.txt");
                zwjFile.setFileName(dir + "/emoji-zwj-sequences.txt");
                break;
            }
        }
    }

    if (!testFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString error = "Cannot open emoji-test.txt: " + testFile.errorString();
        qWarning() << error;
        emit loadFailed(error);
        return false;
    }

    QString testData = QString::fromUtf8(testFile.readAll());
    testFile.close();

    if (!parseEmojiTest(testData)) {
        emit loadFailed("Failed to parse emoji-test.txt");
        return false;
    }

    if (zwjFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString zwjData = QString::fromUtf8(zwjFile.readAll());
        zwjFile.close();
        parseZwjSequences(zwjData);
    }

    buildIndexes();

    qDebug() << "Loaded" << m_emojis.size() << "emojis in" << m_categories.size() << "categories";
    emit loaded();
    return true;
}

bool EmojiDatabase::parseEmojiTest(const QString& data)
{
    QString currentCategory;
    QString currentSubcategory;

    // Unicode emoji-test.txt format:
    // # group: Smileys & Emotion
    // # subgroup: face-smiling
    // 1F600 ; fully-qualified # 😀 grinning face
    // 1F603 ; fully-qualified # 😃 grinning face with big eyes

    static const QRegularExpression groupRe("^#\\s*group:\\s*(.+)$");
    static const QRegularExpression subgroupRe("^#\\s*subgroup:\\s*(.+)$");
    static const QRegularExpression emojiRe("^\\s*([0-9A-Fa-f\\s]+)\\s*;\\s*fully-qualified\\s*#\\s*(.+)\\s+(.+)$");

    QStringList lines = data.split('\n');
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();

        if (trimmed.isEmpty() || ((trimmed.startsWith("# ") && !trimmed.startsWith("# group:") && !trimmed.startsWith("# subgroup:")))) {
            continue;
        }

        QRegularExpressionMatch match;

        if ((match = groupRe.match(trimmed)).hasMatch()) {
            currentCategory = match.captured(1).trimmed();
            if (currentCategory != "Component" && !m_categories.contains(currentCategory)) {
                m_categories.append(currentCategory);
            }
            continue;
        }

        if ((match = subgroupRe.match(trimmed)).hasMatch()) {
            currentSubcategory = match.captured(1).trimmed();
            continue;
        }

        if ((match = emojiRe.match(trimmed)).hasMatch()) {
            QString codepoints = match.captured(1).trimmed();
            QString emojiChar = match.captured(2).trimmed();  // The actual emoji
            QString name = match.captured(3).trimmed();        // Name with keywords

            // Normalize codepoints (uppercase, single spaces)
            codepoints = codepoints.toUpper().simplified();

            // Parse name - format: "grinning face" or "woman: medium skin tone, blond hair"
            // Keywords are often in the name itself
            QStringList keywords = name.split(QRegularExpression("[\\s,:_]+"), Qt::SkipEmptyParts);
            keywords.removeDuplicates();

            // Add the full name as a keyword too
            keywords.prepend(name.toLower());

            EmojiEntry entry;
            entry.codepoints = codepoints;
            entry.name = name;
            entry.keywords = keywords;
            entry.category = currentCategory;
            entry.subcategory = currentSubcategory;
            entry.emojiChar = codepointsToChar(codepoints);

            // Check for skin tone modifiers
            if (codepoints.contains("1F3FB") || codepoints.contains("1F3FC") ||
                codepoints.contains("1F3FD") || codepoints.contains("1F3FE") ||
                codepoints.contains("1F3FF")) {
                entry.hasSkinTones = true;
            }

            // Check for ZWJ sequences
            if (codepoints.contains("200D")) {
                entry.isZwjSequence = true;
            }

            int index = m_emojis.size();
            m_emojis.append(entry);

            // Index by category
            if (!currentCategory.isEmpty()) {
                m_categoryIndex[currentCategory].append(index);
            }

            // Index by keywords
            for (const QString& kw : keywords) {
                m_keywordIndex[kw.toLower()].append(index);
            }
        }
    }

    return true;
}

bool EmojiDatabase::parseZwjSequences(const QString& data)
{
    static const QRegularExpression zwjRe("^\\s*([0-9A-Fa-f\\s]+)\\s*;\\s*fully-qualified\\s*#\\s*(.+)\\s+(.+)$");

    QStringList lines = data.split('\n');
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith("#")) continue;

        QRegularExpressionMatch match;
        if ((match = zwjRe.match(trimmed)).hasMatch()) {
            QString codepoints = match.captured(1).trimmed().toUpper().simplified();
            QString emojiChar = match.captured(2).trimmed();
            QString name = match.captured(3).trimmed();

            QStringList keywords = name.split(QRegularExpression("[\\s,:_]+"), Qt::SkipEmptyParts);
            keywords.removeDuplicates();
            keywords.prepend(name.toLower());

            EmojiEntry entry;
            entry.codepoints = codepoints;
            entry.name = name;
            entry.keywords = keywords;
            entry.category = "Component";  // ZWJ sequences often don't have category
            entry.subcategory = "ZWJ Sequence";
            entry.emojiChar = codepointsToChar(codepoints);
            entry.isZwjSequence = true;

            // Check for skin tones in ZWJ sequences
            if (codepoints.contains("1F3FB") || codepoints.contains("1F3FC") ||
                codepoints.contains("1F3FD") || codepoints.contains("1F3FE") ||
                codepoints.contains("1F3FF")) {
                entry.hasSkinTones = true;
            }

            int index = m_emojis.size();
            m_emojis.append(entry);

            // Index by keywords
            for (const QString& kw : keywords) {
                m_keywordIndex[kw.toLower()].append(index);
            }
        }
    }

    return true;
}

void EmojiDatabase::buildIndexes()
{
    // Use the standard Unicode Emoji category order instead of alphabetical sort.
    // This order matches the emoji-test.txt group headers and what users expect
    // from Plasma/Windows emoji pickers.
    static const QStringList standardOrder = {
        "Smileys & Emotion",
        "People & Body",
        "Animals & Nature",
        "Food & Drink",
        "Travel & Places",
        "Activities",
        "Objects",
        "Symbols",
        "Flags"
    };

    QStringList ordered;
    for (const QString& cat : standardOrder) {
        if (m_categories.contains(cat)) {
            ordered.append(cat);
        }
    }
    // Append any remaining categories not in the standard list (excluding "Component")
    for (const QString& cat : m_categories) {
        if (!ordered.contains(cat) && cat != "Component") {
            ordered.append(cat);
        }
    }
    m_categories = ordered;
}

QString EmojiDatabase::codepointsToChar(const QString& codepoints) const
{
    QStringList parts = codepoints.split(' ', Qt::SkipEmptyParts);
    QString result;
    for (const QString& part : parts) {
        bool ok = false;
        uint codepoint = part.toUInt(&ok, 16);
        if (!ok) continue;
        
        if (codepoint <= 0xFFFF) {
            // BMP character - fits in single QChar
            result.append(QChar(codepoint));
        } else {
            // Supplementary character - needs surrogate pair in UTF-16
            // Use QString::fromUcs4 with char32_t* overload (Qt6 non-deprecated)
            char32_t cp = static_cast<char32_t>(codepoint);
            result.append(QString::fromUcs4(&cp, 1));
        }
    }
    return result;
}

QVector<EmojiEntry> EmojiDatabase::emojisByCategory(const QString& category) const
{
    QVector<EmojiEntry> result;
    auto it = m_categoryIndex.find(category);
    if (it != m_categoryIndex.end()) {
        for (int idx : *it) {
            if (idx >= 0 && idx < m_emojis.size()) {
                result.append(m_emojis[idx]);
            }
        }
    }
    return result;
}

QVector<EmojiEntry> EmojiDatabase::search(const QString& query) const
{
    QString q = query.toLower().trimmed();
    if (q.isEmpty()) return {};

    QVector<int> matchedIndices;
    QSet<int> uniqueIndices;

    // Split query into terms
    QStringList terms = q.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    for (const QString& term : terms) {
        // Exact keyword match
        auto it = m_keywordIndex.find(term);
        if (it != m_keywordIndex.end()) {
            for (int idx : *it) {
                uniqueIndices.insert(idx);
            }
        }

        // Prefix match on keywords
        for (auto kit = m_keywordIndex.begin(); kit != m_keywordIndex.end(); ++kit) {
            if (kit.key().startsWith(term)) {
                for (int idx : *kit) {
                    uniqueIndices.insert(idx);
                }
            }
        }

        // Substring match on name
        for (int i = 0; i < m_emojis.size(); ++i) {
            if (m_emojis[i].name.toLower().contains(term)) {
                uniqueIndices.insert(i);
            }
        }
    }

    // Convert to vector and sort by relevance (exact matches first)
    QVector<EmojiEntry> results;
    for (int idx : uniqueIndices) {
        if (idx >= 0 && idx < m_emojis.size()) {
            results.append(m_emojis[idx]);
        }
    }

    // Sort: exact keyword matches first, then prefix, then substring
    // For simplicity, just return as-is (already somewhat ordered by index)
    return results;
}

const EmojiEntry* EmojiDatabase::findByCodepoints(const QString& codepoints) const
{
    for (const EmojiEntry& entry : m_emojis) {
        if (entry.codepoints == codepoints.toUpper().simplified()) {
            return &entry;
        }
    }
    return nullptr;
}

QVector<EmojiEntry> EmojiDatabase::recentEmojis(const QStringList& recentCodes) const
{
    QVector<EmojiEntry> result;
    for (const QString& code : recentCodes) {
        const EmojiEntry* entry = findByCodepoints(code);
        if (entry) {
            result.append(*entry);
        }
    }
    return result;
}

QVector<EmojiEntry> EmojiDatabase::frequentEmojis(const QVariantMap& frequentCodes) const
{
    QVector<EmojiEntry> result;
    for (auto it = frequentCodes.begin(); it != frequentCodes.end(); ++it) {
        const EmojiEntry* entry = findByCodepoints(it.key());
        if (entry) {
            result.append(*entry);
        }
    }
    // Sort by frequency (value)
    std::sort(result.begin(), result.end(),
        [&frequentCodes](const EmojiEntry& a, const EmojiEntry& b) {
            int freqA = frequentCodes.value(a.codepoints, 0).toInt();
            int freqB = frequentCodes.value(b.codepoints, 0).toInt();
            return freqA > freqB;
        });
    return result;
}

QVector<EmojiEntry> EmojiDatabase::skinToneVariants(const EmojiEntry& base) const
{
    QVector<EmojiEntry> variants;

    if (!base.hasSkinTones && !base.isZwjSequence) {
        // Base emoji without skin tone - find all skin tone variants
        QString baseCodepoints = base.codepoints;
        for (const EmojiEntry& entry : m_emojis) {
            // Check if entry is a skin tone variant of base
            if (entry.codepoints.startsWith(baseCodepoints + " 1F3F") && entry.hasSkinTones) {
                variants.append(entry);
            }
        }
    } else if (base.isZwjSequence) {
        // ZWJ sequence with skin tones - find other skin tone variants
        // This is more complex; for now return empty
    }

    return variants;
}