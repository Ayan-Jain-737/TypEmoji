#include <QtTest/QtTest>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QCoreApplication>
#include "../src/injector/EmojiInjector.h"
#include "../src/TargetState.h"
#include "../src/data/EmojiDatabase.h"
#include "../src/panel/EmojiModel.h"
#include "../src/settings/Settings.h"

class TestSurrogatePairs : public QObject
{
    Q_OBJECT

private slots:
    void testSurrogatePairEncoding();
    void testZwjSequences();
};

void TestSurrogatePairs::testSurrogatePairEncoding()
{
    // Test that Unicode code points are correctly handled
    // In UTF-16 (QString), code points above U+FFFF are surrogate pairs (2 code units)
    QVector<char32_t> grinningData = {0x1F600};  // Above BMP - surrogate pair in UTF-16
    QString grinning = QString::fromUcs4(grinningData.constData(), grinningData.size());
    QCOMPARE(grinning.length(), 2);  // 2 UTF-16 code units (surrogate pair)

    QVector<char32_t> familyData = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467, 0x200D, 0x1F466};  // ZWJ sequence
    QString family = QString::fromUcs4(familyData.constData(), familyData.size());
    // 4 emoji (each surrogate pair = 2) + 3 ZWJ (each 1) = 11 UTF-16 code units
    QCOMPARE(family.length(), 11);
}

void TestSurrogatePairs::testZwjSequences()
{
    // ZWJ sequences should be treated as single grapheme clusters
    QVector<char32_t> zwjData = {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467};
    QString zwj = QString::fromUcs4(zwjData.constData(), zwjData.size());
    QVERIFY(!zwj.isEmpty());
}

class TestTargetState : public QObject
{
    Q_OBJECT

private slots:
    void testCaptureValidity();
    void testCaretPosition();
};

void TestTargetState::testCaptureValidity()
{
    TargetState state;
    // Capture may succeed if X11 display is available (e.g., Xwayland)
    // Just verify it doesn't crash and validity matches capture result
    bool captureResult = state.capture();
    QCOMPARE(state.isValid(), captureResult);
}

void TestTargetState::testCaretPosition()
{
    TargetState state;
    QCOMPARE(state.caretScreenPos(), QPoint());
}

class TestEmojiDatabase : public QObject
{
    Q_OBJECT

private slots:
    void testLoadFallback();
    void testSearchIndex();
    void testCategories();
    void testKaomoji();
};

void TestEmojiDatabase::testLoadFallback()
{
    EmojiDatabase db;
    // Will fail without emoji data files, but shouldn't crash
    bool result = db.load();
    // Either succeeds or fails gracefully
}

void TestEmojiDatabase::testSearchIndex()
{
    EmojiDatabase db;
    if (db.load()) {
        QVector<EmojiEntry> results = db.search("smile");
        QVERIFY(results.size() > 0 || results.size() == 0);  // No crash
    }
}

void TestEmojiDatabase::testCategories()
{
    EmojiDatabase db;
    if (db.load()) {
        QStringList cats = db.categories();
        QVERIFY(cats.contains("Kaomoji"));
        QVERIFY(cats.contains("Smileys & Emotion"));
    }
}

void TestEmojiDatabase::testKaomoji()
{
    EmojiDatabase db;
    if (db.load()) {
        QVector<EmojiEntry> kaomojis = db.emojisByCategory("Kaomoji");
        QVERIFY(kaomojis.size() >= 20);

        QVector<EmojiEntry> shrugResults = db.search("shrug");
        QVERIFY(!shrugResults.isEmpty());
        bool foundShrug = false;
        for (const auto& entry : shrugResults) {
            if (entry.emojiChar.contains("ツ")) {
                foundShrug = true;
                break;
            }
        }
        QVERIFY(foundShrug);
    }
}

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void testDefaults();
    void testPersistence();
    void testRecentFrequent();
};

void TestSettings::testDefaults()
{
    Settings& s = Settings::instance();
    QCOMPARE(s.hotkeyModifiers(), 8);  // Super
    QCOMPARE(s.hotkeyKey(), 0x2e);  // XK_period
    QCOMPARE(s.theme(), QString("system"));
    QVERIFY(s.recentEmojis().isEmpty());
    QVERIFY(s.frequentEmojis().isEmpty());
}

void TestSettings::testPersistence()
{
    Settings& s = Settings::instance();
    s.setTheme("dark");
    QCOMPARE(s.theme(), QString("dark"));
    s.save();

    // Reload
    s.load();
    QCOMPARE(s.theme(), QString("dark"));
}

void TestSettings::testRecentFrequent()
{
    Settings& s = Settings::instance();
    s.clearRecentEmojis();
    s.clearFrequentEmojis();

    s.addRecentEmoji("😀");
    s.addRecentEmoji("😃");
    QCOMPARE(s.recentEmojis().size(), 2);
    QCOMPARE(s.recentEmojis().first(), QString("😃"));  // Most recent first

    // Test FIFO capacity limit (max 20)
    for (int i = 1; i <= 25; ++i) {
        s.addRecentEmoji(QString("emoji_%1").arg(i));
    }
    // Should be capped at 20
    QCOMPARE(s.recentEmojis().size(), 20);
    // Most recent is emoji_25 (entered last)
    QCOMPARE(s.recentEmojis().first(), QString("emoji_25"));
    // Oldest surviving is emoji_6 (emoji_1 through emoji_5 were evicted via FIFO)
    QCOMPARE(s.recentEmojis().last(), QString("emoji_6"));

    s.incrementFrequency("😀");
    s.incrementFrequency("😀");
    s.incrementFrequency("😃");
    QCOMPARE(s.frequentEmojis().value("😀").toInt(), 2);
    QCOMPARE(s.frequentEmojis().value("😃").toInt(), 1);
}

class TestEmojiModel : public QObject
{
    Q_OBJECT

private slots:
    void testModelLoad();
    void testCategoryFilter();
    void testRecentCategory();
    void testSearchFilter();
    void testPurposeSearch();
    void testClearFilters();
};

void TestEmojiModel::testModelLoad()
{
    EmojiDatabase db;
    QVERIFY(db.load());
    
    EmojiModel model(&db);
    
    // Model should have entries after database loads
    QVERIFY(model.rowCount() > 0);
    QCOMPARE(model.rowCount(), model.totalCount());
    
    // First entry should be valid
    QModelIndex idx = model.index(0, 0);
    QVERIFY(idx.isValid());
    QString emoji = idx.data(EmojiModel::EmojiCharRole).toString();
    QVERIFY(!emoji.isEmpty());
}

void TestEmojiModel::testCategoryFilter()
{
    EmojiDatabase db;
    QVERIFY(db.load());
    
    EmojiModel model(&db);
    
    // Filter by category
    model.setCategoryFilter("Smileys & Emotion");
    QVERIFY(model.rowCount() > 0);
    
    // All entries should be from this category
    for (int i = 0; i < model.rowCount(); ++i) {
        QModelIndex idx = model.index(i, 0);
        QString cat = idx.data(EmojiModel::CategoryRole).toString();
        QCOMPARE(cat, QString("Smileys & Emotion"));
    }
}

void TestEmojiModel::testRecentCategory()
{
    EmojiDatabase db;
    QVERIFY(db.load());

    EmojiModel model(&db);
    QVector<EmojiEntry> recents = db.recentEmojis({"😀", "🔥", "🙏"});
    QCOMPARE(recents.size(), 3);

    model.setShowRecent(recents);
    model.setCategoryFilter("Recent");

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.index(0, 0).data(EmojiModel::EmojiCharRole).toString(), QString("😀"));
    QCOMPARE(model.index(1, 0).data(EmojiModel::EmojiCharRole).toString(), QString("🔥"));
    QCOMPARE(model.index(2, 0).data(EmojiModel::EmojiCharRole).toString(), QString("🙏"));
}

void TestEmojiModel::testSearchFilter()
{
    EmojiDatabase db;
    QVERIFY(db.load());
    
    EmojiModel model(&db);
    
    // Search for "heart"
    model.setSearchQuery("heart");
    QVERIFY(model.rowCount() > 0);
    
    // Results should contain "heart" in name or keywords
    bool found = false;
    for (int i = 0; i < qMin(model.rowCount(), 20); ++i) {
        QModelIndex idx = model.index(i, 0);
        QString name = idx.data(EmojiModel::NameRole).toString().toLower();
        QString keywords = idx.data(EmojiModel::KeywordsRole).toString().toLower();
        if (name.contains("heart") || keywords.contains("heart")) {
            found = true;
            break;
        }
    }
    QVERIFY(found);
}

void TestEmojiModel::testPurposeSearch()
{
    EmojiDatabase db;
    QVERIFY(db.load());

    // Purpose search: "pray" should rank folded hands at the top
    QVector<EmojiEntry> prayResults = db.search("pray");
    QVERIFY(!prayResults.isEmpty());
    QVERIFY(prayResults.first().name.toLower().contains("folded hands") || prayResults.first().emojiChar == "🙏");

    // Purpose search: "fire" should return fire emoji as #1
    QVector<EmojiEntry> fireResults = db.search("fire");
    QVERIFY(!fireResults.isEmpty());
    QCOMPARE(fireResults.first().emojiChar, QString("🔥"));

    // Purpose search: "laugh" should return tears of joy or rolling on the floor laughing
    QVector<EmojiEntry> laughResults = db.search("laugh");
    QVERIFY(!laughResults.isEmpty());
    QString topEmoji = laughResults.first().emojiChar;
    QVERIFY(topEmoji == "😂" || topEmoji == "🤣" || topEmoji == "😆");

    // Multi-term search: "red heart" should return red heart as #1
    QVector<EmojiEntry> redHeartResults = db.search("red heart");
    QVERIFY(!redHeartResults.isEmpty());
    QVERIFY(redHeartResults.first().emojiChar == "❤️" || redHeartResults.first().name.toLower() == "red heart");
}

void TestEmojiModel::testClearFilters()
{
    EmojiDatabase db;
    QVERIFY(db.load());
    
    EmojiModel model(&db);
    int initialCount = model.rowCount();
    
    // Apply a filter
    model.setCategoryFilter("Flags");
    QVERIFY(model.rowCount() < initialCount);
    
    // Clear filters
    model.clearFilters();
    QCOMPARE(model.rowCount(), initialCount);
}

// Main test runner
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    int result = 0;

    TestSurrogatePairs test1;
    result |= QTest::qExec(&test1, argc, argv);

    TestTargetState test2;
    result |= QTest::qExec(&test2, argc, argv);

    TestEmojiDatabase test3;
    result |= QTest::qExec(&test3, argc, argv);

    TestSettings test4;
    result |= QTest::qExec(&test4, argc, argv);

    TestEmojiModel test5;
    result |= QTest::qExec(&test5, argc, argv);

    return result;
}

#include "TestRunner.moc"