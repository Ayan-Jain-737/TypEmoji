/**
 * EmojiDatabase implementation - Loads Unicode emoji data
 */
#include "EmojiDatabase.h"
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QRegularExpression>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
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
        QStringList candidateDirs = {
            QCoreApplication::applicationDirPath() + "/data",
            QCoreApplication::applicationDirPath() + "/../share/emojityper/data",
            QDir::homePath() + "/.local/share/emojityper/data",
            "/usr/share/emojityper/data",
            "/usr/local/share/emojityper/data",
            "/app/share/emojityper/data",
            QDir::currentPath() + "/data"
        };
        for (const QString& dataDir : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
            candidateDirs.append(dataDir + "/emojityper/data");
        }
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

    loadKaomoji();
    loadSymbols();
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
    // 1F600 ; fully-qualified # 😀 E1.0 grinning face
    // 1F603 ; fully-qualified # 😃 E0.6 grinning face with big eyes

    static const QRegularExpression groupRe("^#\\s*group:\\s*(.+)$");
    static const QRegularExpression subgroupRe("^#\\s*subgroup:\\s*(.+)$");
    static const QRegularExpression emojiRe("^\\s*([0-9A-Fa-f\\s]+)\\s*;\\s*fully-qualified\\s*#\\s*(\\S+)(?:\\s+E\\d+(?:\\.\\d+)?)?\\s+(.+)$");

    QStringList lines = data.split('\n');
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();

        if (trimmed.isEmpty() || (trimmed.startsWith("# ") && !trimmed.startsWith("# group:") && !trimmed.startsWith("# subgroup:"))) {
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

        if (trimmed.contains("E18.") || trimmed.contains("E19.") || trimmed.contains("E20.")) {
            continue;
        }

        if ((match = emojiRe.match(trimmed)).hasMatch()) {
            QString codepoints = match.captured(1).trimmed().toUpper().simplified();
            QString emojiChar = match.captured(2).trimmed();
            QString name = match.captured(3).trimmed();

            // Filter out unsupported future draft codepoints
            static const QSet<QString> unsupportedCodepoints = {
                "1FAEB", "1FAF9", "1FAFA", "1FACC", "1FADD", "1F6D9", "1FA8B", "1FA8C", "1FA8D"
            };
            bool hasBad = false;
            for (const QString& bad : unsupportedCodepoints) {
                if (codepoints.contains(bad)) {
                    hasBad = true;
                    break;
                }
            }
            if (hasBad) continue;

            // Fallback generation if emojiChar was malformed
            if (emojiChar.isEmpty()) {
                emojiChar = codepointsToChar(codepoints);
            }

            // Parse words from name
            QStringList keywords;
            keywords.append(name.toLower());

            QStringList nameWords = name.toLower().split(QRegularExpression("[\\s,:_\\-]+"), Qt::SkipEmptyParts);
            for (const QString& word : nameWords) {
                keywords.append(word);
                // Simple stemming
                if (word.endsWith("ing") && word.length() > 5) {
                    keywords.append(word.left(word.length() - 3));
                    keywords.append(word.left(word.length() - 3) + "e");
                } else if (word.endsWith("ed") && word.length() > 4) {
                    keywords.append(word.left(word.length() - 2));
                } else if (word.endsWith("es") && word.length() > 4) {
                    keywords.append(word.left(word.length() - 2));
                } else if (word.endsWith('s') && word.length() > 3) {
                    keywords.append(word.left(word.length() - 1));
                }
            }

            // Subcategory words
            if (!currentSubcategory.isEmpty()) {
                QStringList subWords = currentSubcategory.toLower().split(QRegularExpression("[\\s,:_\\-]+"), Qt::SkipEmptyParts);
                keywords.append(subWords);
            }

            EmojiEntry entry;
            entry.codepoints = codepoints;
            entry.name = name;
            entry.category = currentCategory;
            entry.subcategory = currentSubcategory;
            entry.emojiChar = emojiChar;

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

            entry.keywords = keywords;
            attachPurposeKeywords(entry);
            entry.keywords.removeDuplicates();

            int index = m_emojis.size();
            m_emojis.append(entry);

            // Fast hash indices
            m_codepointsIndex.insert(codepoints, index);
            m_charIndex.insert(emojiChar, index);

            // Index by category
            if (!currentCategory.isEmpty()) {
                m_categoryIndex[currentCategory].append(index);
            }

            // Index by keywords
            for (const QString& kw : entry.keywords) {
                m_keywordIndex[kw.toLower()].append(index);
            }
        }
    }

    return true;
}

bool EmojiDatabase::parseZwjSequences(const QString& data)
{
    // emoji-test.txt already includes all fully-qualified sequences with their correct groups.
    // parseZwjSequences adds any missing standalone sequences if needed.
    static const QRegularExpression zwjRe("^\\s*([0-9A-Fa-f\\s]+)\\s*;\\s*(?:RGI_Emoji_ZWJ_Sequence|fully-qualified)\\s*;?\\s*([^#]+)#\\s*(\\S+)?(?:\\s+E\\d+(?:\\.\\d+)?)?\\s*(.+)$");

    QStringList lines = data.split('\n');
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith("#")) continue;

        QRegularExpressionMatch match = zwjRe.match(trimmed);
        if (match.hasMatch()) {
            QString codepoints = match.captured(1).trimmed().toUpper().simplified();
            if (m_codepointsIndex.contains(codepoints)) {
                continue; // Already loaded from emoji-test.txt
            }

            QString name = match.captured(2).trimmed();
            if (name.isEmpty()) {
                name = match.captured(4).trimmed();
            }

            QString emojiChar = codepointsToChar(codepoints);

            EmojiEntry entry;
            entry.codepoints = codepoints;
            entry.name = name;
            entry.category = "Component";
            entry.subcategory = "ZWJ Sequence";
            entry.emojiChar = emojiChar;
            entry.isZwjSequence = true;

            if (codepoints.contains("1F3FB") || codepoints.contains("1F3FC") ||
                codepoints.contains("1F3FD") || codepoints.contains("1F3FE") ||
                codepoints.contains("1F3FF")) {
                entry.hasSkinTones = true;
            }

            QStringList keywords;
            keywords.append(name.toLower());
            keywords.append(name.toLower().split(QRegularExpression("[\\s,:_\\-]+"), Qt::SkipEmptyParts));
            entry.keywords = keywords;
            attachPurposeKeywords(entry);
            entry.keywords.removeDuplicates();

            int index = m_emojis.size();
            m_emojis.append(entry);
            m_codepointsIndex.insert(codepoints, index);
            m_charIndex.insert(emojiChar, index);

            for (const QString& kw : entry.keywords) {
                m_keywordIndex[kw.toLower()].append(index);
            }
        }
    }

    return true;
}

void EmojiDatabase::attachPurposeKeywords(EmojiEntry& entry)
{
    static const QHash<QString, QStringList> purposeMap = {
        // Emotions & Reactions
        {"1F600", {"smile", "happy", "grin", "joy", "cheerful"}},
        {"1F603", {"smile", "happy", "joy", "big eyes", "cheerful"}},
        {"1F604", {"smile", "happy", "joy", "smiling eyes", "pleased"}},
        {"1F601", {"grin", "smiling", "happy", "teeth", "excited"}},
        {"1F606", {"laugh", "laughing", "lol", "haha", "squint", "giggle"}},
        {"1F605", {"sweat", "nervous", "relief", "whew", "awkward", "phew", "embarrassed"}},
        {"1F923", {"rofl", "lmao", "lol", "laugh", "laughing", "haha", "rolling on floor", "dying", "dead"}},
        {"1F602", {"laugh", "laughing", "lol", "lmao", "haha", "crying", "funny", "joy", "rofl", "tears of joy", "humor"}},
        {"1F642", {"smile", "mild", "content", "okay", "fine"}},
        {"1F643", {"upside down", "sarcasm", "irony", "silly", "goofy"}},
        {"1FAE0", {"melt", "melting", "embarrassed", "overwhelmed", "hot", "sarcastic", "disappearing"}},
        {"1F609", {"wink", "flirt", "playful", "joke", "subtle"}},
        {"1F60A", {"blush", "smile", "warm", "flattered", "shy", "happy"}},
        {"1F607", {"angel", "halo", "innocent", "pure", "saint"}},
        {"1F970", {"love", "crush", "loved", "hearts", "adore", "affection", "cute"}},
        {"1F60D", {"love", "heart", "crush", "in love", "adore", "beautiful", "gorgeous"}},
        {"1F929", {"star struck", "star eyes", "amazed", "wow", "famous", "excited", "celebrity"}},
        {"1F618", {"kiss", "love", "smooch", "blow kiss", "flirt", "affection"}},
        {"1F617", {"kiss", "duck face", "smooch"}},
        {"1F61A", {"kiss", "blush", "smooch", "affection"}},
        {"1F619", {"kiss", "smile", "smooch"}},
        {"1F60B", {"yummy", "delicious", "tasty", "savoring", "tongue", "nom"}},
        {"1F61B", {"tongue", "playful", "cheeky", "silly"}},
        {"1F61C", {"wink tongue", "crazy", "party", "wild", "silly"}},
        {"1F92A", {"zany", "crazy", "wild", "goofy", "silly"}},
        {"1F61D", {"squint tongue", "playful", "haha", "silly"}},
        {"1F911", {"money", "cash", "dollar", "rich", "wealth", "paid", "greedy", "profit"}},
        {"1F917", {"hug", "hugging", "warmth", "care", "welcome", "open arms"}},
        {"1FAC2", {"hug", "comfort", "support", "embrace", "friends", "love"}},
        {"1F92D", {"giggle", "oops", "chuckle", "covering mouth", "secret"}},
        {"1F92B", {"shh", "quiet", "secret", "hush", "silence", "whisper"}},
        {"1F914", {"think", "thinking", "hmm", "ponder", "wonder", "curious", "idea", "skeptical"}},
        {"1FAE1", {"salute", "respect", "yes sir", "honor", "roger", "acknowledged"}},
        {"1F910", {"zip", "quiet", "secret", "shut up", "silent", "lips sealed"}},
        {"1F928", {"eyebrow", "skeptical", "suspicious", "doubt", "distrust"}},
        {"1F610", {"neutral", "blank", "meh", "indifferent", "straight face"}},
        {"1F611", {"expressionless", "unimpressed", "done", "meh", "blank"}},
        {"1F636", {"no mouth", "silent", "quiet", "speechless"}},
        {"1F60F", {"smirk", "flirt", "suggestive", "sly", "cheeky", "satisfied"}},
        {"1F612", {"unamused", "bored", "annoyed", "dissatisfied", "side eye"}},
        {"1F644", {"roll eyes", "eyeroll", "annoyed", "whatever", "sarcastic", "bored"}},
        {"1F62C", {"grimace", "awkward", "yikes", "nervous", "cringe"}},
        {"1F92E", {"vomit", "puke", "throw up", "gross", "sick", "disgusting"}},
        {"1F922", {"sick", "nausea", "gross", "green", "ill", "disgusted"}},
        {"1F634", {"sleep", "sleepy", "tired", "zzz", "bed", "night", "asleep"}},
        {"1F971", {"yawn", "tired", "sleepy", "bored", "exhausted"}},
        {"1F637", {"mask", "sick", "covid", "doctor", "health", "ill"}},
        {"1F912", {"thermometer", "fever", "sick", "ill", "temperature"}},
        {"1F915", {"bandage", "hurt", "injured", "headache", "wound"}},
        {"1F92F", {"mind blown", "shocked", "omg", "woah", "boom", "amazed", "brain"}},
        {"1F975", {"hot", "sweat", "spicy", "summer", "exhausted", "heat"}},
        {"1F976", {"cold", "freezing", "ice", "frost", "winter", "chilly"}},
        {"1F974", {"woozy", "drunk", "dizzy", "tipsy", "hungover"}},
        {"1F635", {"knocked out", "dizzy", "fainted", "dead eyes"}},
        {"1F60E", {"cool", "sunglasses", "swag", "chill", "boss", "slick"}},
        {"1F913", {"nerd", "geek", "smart", "glasses", "study", "dork"}},
        {"1F9D0", {"monocle", "smart", "hmm", "inspect", "scrutinize", "curious"}},
        {"1F615", {"confused", "huh", "puzzled", "uncertain"}},
        {"1F61F", {"worried", "concerned", "nervous", "anxious"}},
        {"1F641", {"frown", "sad", "disappointed", "unhappy"}},
        {"2639",  {"frown", "sad", "upset", "unhappy"}},
        {"1F62E", {"gasp", "surprised", "whoa", "shocked", "open mouth"}},
        {"1F62F", {"surprised", "hushed", "speechless", "astonished"}},
        {"1F632", {"astonished", "shocked", "disbelief", "omg"}},
        {"1F633", {"flushed", "embarrassed", "blushing", "shocked", "wide eyes"}},
        {"1F97A", {"plead", "please", "puppy eyes", "begging", "cute", "simp"}},
        {"1F979", {"holding back tears", "moved", "proud", "emotional", "gratitude"}},
        {"1F626", {"frowning", "aww", "sad", "anguish"}},
        {"1F627", {"anguished", "shocked", "worried"}},
        {"1F628", {"fearful", "scared", "nervous", "anxious"}},
        {"1F630", {"cold sweat", "scared", "worried", "frantic"}},
        {"1F625", {"sad", "disappointed", "relief", "tear", "upset"}},
        {"1F622", {"cry", "crying", "tear", "sad", "unhappy", "depressed"}},
        {"1F62D", {"cry", "crying", "sob", "sobbing", "tears", "sad", "unhappy", "bawling", "depressed"}},
        {"1F631", {"screaming", "scared", "terror", "shock", "horror", "munch"}},
        {"1F616", {"confounded", "frustrated", "struggling"}},
        {"1F623", {"persevering", "struggling", "enduring"}},
        {"1F61E", {"disappointed", "sad", "down", "gloomy"}},
        {"1F613", {"sweat", "tired", "downcast", "exhausted"}},
        {"1F629", {"weary", "tired", "stressed", "overwhelmed"}},
        {"1F62B", {"tired", "exhausted", "done", "whining"}},
        {"1F624", {"triumph", "proud", "huffing", "steam", "determined"}},
        {"1F621", {"angry", "mad", "rage", "furious", "curse", "pissed", "red"}},
        {"1F620", {"angry", "mad", "grumpy", "annoyed"}},
        {"1F92C", {"curse", "swear", "profanity", "censored", "furious", "angry"}},
        {"1F608", {"evil", "devil", "horns", "smile", "naughty"}},
        {"1F47F", {"devil", "angry", "evil", "demon", "imp"}},
        {"1F480", {"dead", "dying", "skeleton", "rip", "death", "skull", "corpse", "laughter"}},
        {"2620",  {"poison", "danger", "pirate", "deadly", "skull"}},
        {"1F4A9", {"poop", "shit", "crap", "turd", "feces"}},
        {"1F921", {"clown", "circus", "fool", "joke", "silly", "joker"}},
        {"1F47B", {"ghost", "spooky", "boo", "halloween", "spirit"}},
        {"1F47D", {"alien", "ufo", "extraterrestrial", "space"}},
        {"1F916", {"robot", "bot", "ai", "machine", "tech"}},

        // Gestures & Body
        {"1F44B", {"wave", "hello", "hi", "bye", "goodbye", "welcome", "greetings"}},
        {"1F91A", {"back of hand", "wave", "stop"}},
        {"1F590", {"hand", "five", "stop", "palm"}},
        {"270B",  {"hand", "stop", "high five", "wait", "palm"}},
        {"1F596", {"vulcan", "spock", "star trek", "live long"}},
        {"1F44C", {"ok", "perfect", "good", "fine", "agree", "chef kiss"}},
        {"1F90F", {"pinching", "little", "small", "tiny"}},
        {"270C",  {"peace", "v", "victory", "two"}},
        {"1F91E", {"fingers crossed", "luck", "hope", "promise", "wish"}},
        {"1FAF0", {"pinched fingers", "italian", "gesture", "what"}},
        {"1F91F", {"love you", "rock on", "gesture"}},
        {"1F918", {"rock on", "metal", "devil horns", "cool", "party"}},
        {"1F919", {"call me", "shaka", "hang loose", "phone"}},
        {"1F448", {"point left", "left", "direction"}},
        {"1F449", {"point right", "right", "direction"}},
        {"1F446", {"point up", "up", "above", "this"}},
        {"1F595", {"middle finger", "fu", "offensive"}},
        {"1F447", {"point down", "down", "below"}},
        {"261D",  {"point up", "index", "attention", "one"}},
        {"1F44D", {"like", "approve", "ok", "yes", "good", "+1", "agree", "accept", "positive", "thumbs up"}},
        {"1F44E", {"dislike", "disapprove", "no", "bad", "-1", "reject", "negative", "thumbs down"}},
        {"270A",  {"fist", "power", "strength", "solidarity"}},
        {"1F44A", {"fist", "punch", "brofist", "power", "strength", "fight"}},
        {"1F44F", {"clap", "applause", "bravo", "praise", "props", "congrats"}},
        {"1F64C", {"praise", "hooray", "hands up", "celebrate", "raised hands"}},
        {"1FAF6", {"heart hands", "love", "care", "affection", "support", "appreciate"}},
        {"1F450", {"open hands", "hug", "give"}},
        {"1F932", {"palms together", "pray", "prayer", "begging"}},
        {"1F91D", {"handshake", "deal", "agree", "partner", "shake", "meeting"}},
        {"1F64F", {"pray", "praying", "prayer", "please", "thank you", "thanks", "namaste", "hope", "blessing", "high five", "respect", "folded hands"}},
        {"270D",  {"write", "pencil", "note", "writing"}},
        {"1F485", {"nails", "polish", "manicure", "sassy", "chill"}},
        {"1F933", {"selfie", "camera", "phone", "photo"}},
        {"1F4AA", {"muscle", "strong", "flex", "workout", "fitness", "bicep", "power"}},
        {"1F440", {"eyes", "look", "see", "watch", "peeping", "shifty", "curious", "spying"}},
        {"1F441", {"eye", "look", "vision"}},
        {"1F445", {"tongue", "taste", "lick"}},
        {"1F444", {"mouth", "lips", "kiss"}},
        {"1F48B", {"kiss", "lipstick", "smooch", "lips", "love"}},
        {"1F937", {"shrug", "idk", "dunno", "whatever", "who knows", "doubt"}},
        {"1F926", {"facepalm", "frustrated", "disbelief", "smh", "sigh", "embarrassed"}},

        // Hearts & Love
        {"2764",  {"heart", "love", "like", "favorite", "valentine", "care", "romance", "red heart"}},
        {"1F9E1", {"orange heart", "love", "friendship"}},
        {"1F49B", {"yellow heart", "love", "friendship", "gold"}},
        {"1F49A", {"green heart", "love", "nature", "jealous"}},
        {"1F499", {"blue heart", "love", "trust", "loyalty"}},
        {"1F49C", {"purple heart", "love", "royalty", "bts"}},
        {"1F90E", {"brown heart", "love", "coffee", "chocolate"}},
        {"1F5A4", {"black heart", "love", "dark", "goth", "sorrow"}},
        {"1F90D", {"white heart", "love", "pure", "peace"}},
        {"1FA75", {"grey heart", "love", "silver"}},
        {"1FA77", {"pink heart", "love", "sweet", "cute"}},
        {"1F494", {"broken heart", "heartbreak", "sad", "breakup", "hurt"}},
        {"2763",  {"heart exclamation", "love"}},
        {"1F495", {"two hearts", "love", "romance"}},
        {"1F49E", {"revolving hearts", "love", "romance"}},
        {"1F493", {"beating heart", "love", "heartbeat"}},
        {"1F497", {"growing heart", "love", "affection"}},
        {"1F496", {"sparkling heart", "love", "shine"}},
        {"1F498", {"heart with arrow", "cupid", "love"}},
        {"1F49D", {"heart with ribbon", "gift", "love"}},

        // Popular Symbols, Food, Nature, Objects
        {"1F525", {"fire", "hot", "lit", "flame", "burn", "trending", "cool", "hype"}},
        {"2728",  {"sparkle", "stars", "glitter", "magic", "clean", "shiny", "ai", "gemini", "new", "special"}},
        {"2B50",  {"star", "favorite", "rate", "gold", "rating", "stellar"}},
        {"1F31F", {"glowing star", "shining", "bright", "star"}},
        {"1F4A5", {"collision", "boom", "explode", "bang", "pow"}},
        {"1F4AF", {"100", "hundred", "perfect", "score", "keep it 100", "real", "truth", "facts"}},
        {"1F389", {"party", "popper", "tada", "congrats", "celebrate", "birthday", "winner"}},
        {"1F388", {"balloon", "party", "birthday", "celebrate"}},
        {"1F382", {"cake", "birthday", "party", "dessert", "sweet"}},
        {"1F381", {"gift", "present", "birthday", "christmas"}},
        {"1F3C6", {"trophy", "winner", "prize", "first", "champion", "victory", "cup"}},
        {"1F947", {"gold medal", "medal", "first", "winner", "champion", "1st"}},
        {"1F451", {"crown", "king", "queen", "royal", "royalty", "leader", "champion"}},
        {"1F4B0", {"money", "cash", "dollar", "rich", "wealth", "paid", "loot", "fortune", "bag"}},
        {"1F4B5", {"dollar", "cash", "money", "currency", "bill"}},
        {"1F680", {"rocket", "launch", "moon", "to the moon", "fast", "space", "fly", "speed", "crypto", "stonks"}},
        {"1F4A1", {"idea", "light", "bulb", "bright", "smart", "think", "eureka"}},
        {"1F514", {"bell", "notification", "ring", "alert", "subscribe"}},
        {"26A0",  {"warning", "caution", "alert", "danger", "hazard", "careful"}},
        {"2705",  {"check", "tick", "done", "correct", "verified", "ok", "yes", "success", "pass"}},
        {"2714",  {"check", "tick", "done", "correct", "verified", "ok", "yes"}},
        {"274C",  {"cross", "wrong", "x", "no", "cancel", "deny", "stop", "fail", "error", "rejected"}},
        {"1F436", {"dog", "puppy", "canine", "pet", "doggo", "bark"}},
        {"1F415", {"dog", "canine", "pet", "walk"}},
        {"1F431", {"cat", "kitten", "kitty", "feline", "pet", "meow"}},
        {"1F408", {"cat", "feline", "pet"}},
        {"1F410", {"goat", "greatest of all time", "animal"}},
        {"2615",  {"coffee", "caffeine", "espresso", "latte", "tea", "morning", "drink", "break"}},
        {"1F37A", {"beer", "cheers", "drink", "alcohol", "party", "bar", "pub"}},
        {"1F37B", {"beers", "cheers", "toast", "alcohol", "party", "drinking"}},
        {"1F355", {"pizza", "food", "slice", "cheese", "italian", "eat", "lunch", "dinner"}},
        {"1F354", {"burger", "hamburger", "fast food", "beef", "lunch", "eat"}},
        {"1F35F", {"fries", "french fries", "fast food", "potato"}},
        {"1F34E", {"apple", "fruit", "red", "healthy"}},
        {"1F34C", {"banana", "fruit", "yellow"}},
        {"1F33F", {"herb", "leaf", "plant", "green"}},
        {"1F340", {"clover", "four leaf", "lucky", "luck", "irish"}},
        {"1F338", {"cherry blossom", "flower", "sakura", "pink", "spring"}},
        {"1F339", {"rose", "flower", "love", "red"}},
        {"2600",  {"sun", "sunny", "weather", "bright", "day"}},
        {"2601",  {"cloud", "weather", "cloudy", "sky"}},
        {"26A1",  {"lightning", "thunder", "electric", "power", "fast", "bolt"}},
        {"2744",  {"snowflake", "winter", "cold", "snow", "ice"}},
        {"1F697", {"car", "automobile", "drive", "vehicle"}},
        {"1F4BB", {"laptop", "computer", "pc", "tech", "code", "work"}},
        {"1F4F1", {"phone", "iphone", "mobile", "cell", "smartphone"}}
    };

    QString base = entry.codepoints.section(' ', 0, 0);
    auto it = purposeMap.constFind(base);
    if (it != purposeMap.constEnd()) {
        for (const QString& tag : it.value()) {
            if (!entry.keywords.contains(tag, Qt::CaseInsensitive)) {
                entry.keywords.prepend(tag);
            }
        }
    }

    if (base != entry.codepoints) {
        auto itFull = purposeMap.constFind(entry.codepoints);
        if (itFull != purposeMap.constEnd()) {
            for (const QString& tag : itFull.value()) {
                if (!entry.keywords.contains(tag, Qt::CaseInsensitive)) {
                    entry.keywords.prepend(tag);
                }
            }
        }
    }
}

void EmojiDatabase::buildIndexes()
{
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
    for (const QString& cat : m_categories) {
        if (!ordered.contains(cat) && cat != "Component") {
            ordered.append(cat);
        }
    }
    m_categories = ordered;

    // Mark base emojis that support skin tone variants
    static const QStringList toneCodes = {"1F3FB", "1F3FC", "1F3FD", "1F3FE", "1F3FF"};
    for (const auto& entry : m_emojis) {
        if (entry.hasSkinTones) {
            QStringList ep = entry.codepoints.split(' ', Qt::SkipEmptyParts);
            QStringList eb;
            for (const QString& p : ep) {
                if (!toneCodes.contains(p)) eb.append(p);
            }
            QString baseCps = eb.join(' ');
            auto it = m_codepointsIndex.find(baseCps);
            if (it != m_codepointsIndex.end() && it.value() >= 0 && it.value() < m_emojis.size()) {
                m_emojis[it.value()].hasSkinTones = true;
            }
        }
    }

    // Build search vocabulary for rapid typo-tolerant fuzzy matching
    m_searchCache.clear();
    m_vocabList.clear();

    QHash<QString, QVector<WordPosting>> vocab;
    for (int i = 0; i < m_emojis.size(); ++i) {
        const auto& e = m_emojis[i];
        QString nameLower = e.name.toLower();
        auto nameWords = nameLower.split(u' ', Qt::SkipEmptyParts);

        vocab[nameLower].append({i, WordPosting::ExactName});

        for (int wIdx = 0; wIdx < nameWords.size(); ++wIdx) {
            const QString& w = nameWords[wIdx];
            WordPosting::Role r = (wIdx == 0 || nameWords.size() <= 2) ? WordPosting::FirstNameWord : WordPosting::NameWord;
            vocab[w].append({i, r});
        }
        for (int kIdx = 0; kIdx < e.keywords.size(); ++kIdx) {
            const QString& kw = e.keywords[kIdx].toLower();
            if (kw != nameLower) {
                WordPosting::Role r = WordPosting::Keyword;
                if (kIdx == 0 || (kw == "heart" && e.codepoints.startsWith("2764"))) r = WordPosting::ExactName;
                else if (kIdx < 3) r = WordPosting::PrimaryKeyword;
                vocab[kw].append({i, r});
            }
        }
    }

    m_vocabList.reserve(vocab.size());
    for (auto it = vocab.begin(); it != vocab.end(); ++it) {
        m_vocabList.append({it.key(), it.value()});
    }
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
            result.append(QChar(codepoint));
        } else {
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

int EmojiDatabase::damerauLevenshtein(QStringView s1, QStringView s2, int maxDist)
{
    int len1 = static_cast<int>(s1.length());
    int len2 = static_cast<int>(s2.length());
    if (std::abs(len1 - len2) > maxDist) return maxDist + 1;
    if (len1 == 0) return len2 <= maxDist ? len2 : maxDist + 1;
    if (len2 == 0) return len1 <= maxDist ? len1 : maxDist + 1;
    if (len1 > 32 || len2 > 32) return maxDist + 1;

    int d[33][33];
    for (int i = 0; i <= len1; ++i) d[i][0] = i;
    for (int j = 0; j <= len2; ++j) d[0][j] = j;

    for (int i = 1; i <= len1; ++i) {
        int minRow = d[i][0];
        QChar c1 = s1[i - 1];
        for (int j = 1; j <= len2; ++j) {
            QChar c2 = s2[j - 1];
            int cost = (c1 == c2) ? 0 : 1;
            int val = std::min({
                d[i - 1][j] + 1,       // deletion
                d[i][j - 1] + 1,       // insertion
                d[i - 1][j - 1] + cost // substitution
            });
            if (i > 1 && j > 1 && c1 == s2[j - 2] && s1[i - 2] == c2) {
                val = std::min(val, d[i - 2][j - 2] + 1); // transposition
            }
            d[i][j] = val;
            minRow = std::min(minRow, val);
        }
        if (minRow > maxDist) return maxDist + 1;
    }
    return d[len1][len2];
}

int EmojiDatabase::calculateMatchScore(const QString& query, const QString& name, const QStringList& keywords, bool hasSkinTones, const QString& category)
{
    QString q = query.trimmed().toLower();
    if (q.isEmpty()) return 0;

    static const QHash<QString, QString> commonTypos = {
        {"hart", "heart"},
        {"smlie", "smile"},
        {"thunb", "thumb"},
        {"cryin", "crying"},
        {"firee", "fire"}
    };

    QString aliasTarget = commonTypos.value(q);
    int qLen = static_cast<int>(q.length());
    int maxDist = (qLen >= 5) ? 2 : ((qLen >= 3) ? 1 : 0);

    QString nameLower = name.toLower();
    if (nameLower == q) {
        return 12000 + (!hasSkinTones ? 400 : 0) + (category != "Kaomoji" ? 300 : 0) + std::max(0, 60 - static_cast<int>(nameLower.length()));
    }
    if (!aliasTarget.isEmpty() && nameLower == aliasTarget) {
        return 11500 + (!hasSkinTones ? 400 : 0) + (category != "Kaomoji" ? 300 : 0) + std::max(0, 60 - static_cast<int>(nameLower.length()));
    }

    int bestScore = 0;

    // Full name prefix
    if (nameLower.startsWith(q)) {
        bestScore = std::max(bestScore, 8500 + std::max(0, static_cast<int>(300 - (nameLower.length() - qLen) * 20)));
    } else if (!aliasTarget.isEmpty() && nameLower.startsWith(aliasTarget)) {
        bestScore = std::max(bestScore, 8200 + std::max(0, static_cast<int>(200 - (nameLower.length() - aliasTarget.length()) * 20)));
    }

    auto nameWords = nameLower.split(u' ', Qt::SkipEmptyParts);

    // Check name words
    for (int wIdx = 0; wIdx < nameWords.size(); ++wIdx) {
        const QString& w = nameWords[wIdx];
        int wLen = static_cast<int>(w.length());
        int roleBonus = (wIdx == 0 || nameWords.size() <= 2) ? 600 : 350;

        if (w == q) {
            bestScore = std::max(bestScore, 9500 + roleBonus);
        } else if (!aliasTarget.isEmpty() && (w == aliasTarget || w == aliasTarget + u's')) {
            bestScore = std::max(bestScore, 8800 + roleBonus);
        } else if (w.startsWith(q)) {
            int score = 8000 + std::max(0, static_cast<int>(300 - (wLen - qLen) * 20)) + roleBonus;
            bestScore = std::max(bestScore, score);
        } else if (!aliasTarget.isEmpty() && w.startsWith(aliasTarget)) {
            int score = 7800 + std::max(0, static_cast<int>(200 - (wLen - aliasTarget.length()) * 20)) + roleBonus;
            bestScore = std::max(bestScore, score);
        } else if (maxDist > 0) {
            int dist = damerauLevenshtein(q, w, maxDist);
            if (dist <= maxDist) {
                int s = (dist == 1 ? 5200 : 3200);
                if (wLen > 0 && qLen > 0 && w[0] == q[0]) {
                    s += 300;
                    if (wLen > 1 && qLen > 1 && w[1] == q[1]) s += 200;
                }
                s -= std::abs(wLen - qLen) * 50;
                bestScore = std::max(bestScore, s + roleBonus);
            } else if (wLen > qLen) {
                int pDist = damerauLevenshtein(q, QStringView(w).left(qLen), maxDist);
                if (pDist <= maxDist) {
                    int s = (pDist == 1 ? 4600 : 2800);
                    if (w[0] == q[0]) s += 200;
                    bestScore = std::max(bestScore, s + roleBonus);
                }
            }
        }
    }

    // Check keywords
    for (int kIdx = 0; kIdx < keywords.size(); ++kIdx) {
        const QString& kw = keywords[kIdx].toLower();
        if (kw == nameLower) continue;
        int kwLen = static_cast<int>(kw.length());
        int roleBonus = (kIdx == 0) ? 700 : ((kIdx < 3) ? 550 : 150);

        if (kw == q) {
            bestScore = std::max(bestScore, 9500 + roleBonus);
        } else if (!aliasTarget.isEmpty() && (kw == aliasTarget || kw == aliasTarget + u's')) {
            bestScore = std::max(bestScore, 8800 + roleBonus);
        } else if (kw.startsWith(q)) {
            int score = 8000 + std::max(0, static_cast<int>(300 - (kwLen - qLen) * 20)) + roleBonus;
            bestScore = std::max(bestScore, score);
        } else if (!aliasTarget.isEmpty() && kw.startsWith(aliasTarget)) {
            int score = 7800 + std::max(0, static_cast<int>(200 - (kwLen - aliasTarget.length()) * 20)) + roleBonus;
            bestScore = std::max(bestScore, score);
        } else if (maxDist > 0) {
            int dist = damerauLevenshtein(q, kw, maxDist);
            if (dist <= maxDist) {
                int s = (dist == 1 ? 5200 : 3200);
                if (kwLen > 0 && qLen > 0 && kw[0] == q[0]) {
                    s += 300;
                    if (kwLen > 1 && qLen > 1 && kw[1] == q[1]) s += 200;
                }
                s -= std::abs(kwLen - qLen) * 50;
                bestScore = std::max(bestScore, s + roleBonus);
            } else if (kwLen > qLen && !kw.contains(u' ')) {
                int pDist = damerauLevenshtein(q, QStringView(kw).left(qLen), maxDist);
                if (pDist <= maxDist) {
                    int s = (pDist == 1 ? 4600 : 2800);
                    if (kw[0] == q[0]) s += 200;
                    bestScore = std::max(bestScore, s + roleBonus);
                }
            }
        }
    }

    if (bestScore > 0) {
        if (!hasSkinTones) bestScore += 400;
        if (category != "Kaomoji") bestScore += 300;
        bestScore += std::max(0, 60 - static_cast<int>(nameLower.length()));
    }

    return bestScore;
}

QHash<int, int> EmojiDatabase::searchSingleTerm(const QString& term) const
{
    QString q = term.toLower().trimmed();
    if (q.isEmpty()) return {};

    static const QHash<QString, QString> commonTypos = {
        {"hart", "heart"},
        {"smlie", "smile"},
        {"thunb", "thumb"},
        {"cryin", "crying"},
        {"firee", "fire"}
    };

    QString aliasTarget = commonTypos.value(q);
    int qLen = static_cast<int>(q.length());
    int maxDist = (qLen >= 5) ? 2 : ((qLen >= 3) ? 1 : 0);

    QHash<int, int> scores;
    scores.reserve(128);

    for (const auto& wEntry : m_vocabList) {
        const QString& w = wEntry.word;
        int wLen = static_cast<int>(w.length());
        int baseScore = 0;
        bool isExact = false;

        if (w == q) {
            baseScore = 9500;
            isExact = true;
        } else if (!aliasTarget.isEmpty() && (w == aliasTarget || w == aliasTarget + u's')) {
            baseScore = 9000;
            isExact = true;
        } else if (w.startsWith(q)) {
            baseScore = 8000;
            baseScore += std::max(0, static_cast<int>(300 - (wLen - qLen) * 20));
        } else if (!aliasTarget.isEmpty() && w.startsWith(aliasTarget)) {
            baseScore = 7800;
            baseScore += std::max(0, static_cast<int>(200 - (wLen - aliasTarget.length()) * 20));
        } else if (maxDist > 0) {
            int dist = damerauLevenshtein(q, w, maxDist);
            if (dist <= maxDist) {
                baseScore = (dist == 1 ? 5200 : 3200);
                if (wLen > 0 && qLen > 0 && w[0] == q[0]) {
                    baseScore += 300;
                    if (wLen > 1 && qLen > 1 && w[1] == q[1]) baseScore += 200;
                }
                baseScore -= std::abs(wLen - qLen) * 50;
            } else if (wLen > qLen && !w.contains(u' ')) {
                int pDist = damerauLevenshtein(q, QStringView(w).left(qLen), maxDist);
                if (pDist <= maxDist) {
                    baseScore = (pDist == 1 ? 4600 : 2800);
                    if (w[0] == q[0]) baseScore += 200;
                }
            }
        }

        if (baseScore > 0) {
            for (const auto& post : wEntry.postings) {
                int score = baseScore;
                switch (post.role) {
                case WordPosting::ExactName:
                    score += isExact ? 1500 : 300;
                    break;
                case WordPosting::FirstNameWord:
                    score += 600;
                    break;
                case WordPosting::PrimaryKeyword:
                    score += 550;
                    break;
                case WordPosting::NameWord:
                    score += 350;
                    break;
                case WordPosting::Keyword:
                    score += 150;
                    break;
                }

                int& curr = scores[post.emojiIndex];
                if (score > curr) curr = score;
            }
        }
    }
    return scores;
}

QVector<int> EmojiDatabase::searchIndices(const QString& query) const
{
    QString q = query.toLower().trimmed();
    if (q.isEmpty()) return {};

    // Check search cache for instant response (< 0.001ms)
    auto cacheIt = m_searchCache.constFind(q);
    if (cacheIt != m_searchCache.constEnd()) {
        return cacheIt.value();
    }

    // Direct character or codepoint match (e.g. pasted emoji)
    auto charIt = m_charIndex.constFind(q);
    if (charIt != m_charIndex.constEnd()) {
        QVector<int> res = {charIt.value()};
        m_searchCache.insert(q, res);
        return res;
    }
    auto cpIt = m_codepointsIndex.constFind(q.toUpper());
    if (cpIt != m_codepointsIndex.constEnd()) {
        QVector<int> res = {cpIt.value()};
        m_searchCache.insert(q, res);
        return res;
    }

    QStringList terms = q.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (terms.isEmpty()) return {};

    struct Candidate {
        int index;
        int score;
    };
    QVector<Candidate> results;

    if (terms.size() == 1) {
        QHash<int, int> termScores = searchSingleTerm(terms[0]);
        results.reserve(termScores.size());
        for (auto it = termScores.begin(); it != termScores.end(); ++it) {
            int idx = it.key();
            int score = it.value();
            const auto& e = m_emojis[idx];
            if (!e.hasSkinTones) score += 400;
            if (e.category != "Kaomoji") score += 300;
            score += std::max(0, 60 - static_cast<int>(e.name.length()));
            results.append({idx, score});
        }
    } else {
        // Multi-term query
        QVector<QHash<int, int>> termScoresList;
        termScoresList.reserve(terms.size());
        for (const auto& term : terms) {
            termScoresList.append(searchSingleTerm(term));
        }

        QHash<int, int> combinedScores;
        QHash<int, int> termsMatchedCount;

        for (const auto& tMap : termScoresList) {
            for (auto it = tMap.begin(); it != tMap.end(); ++it) {
                combinedScores[it.key()] += it.value();
                termsMatchedCount[it.key()] += 1;
            }
        }

        bool hasAllMatched = false;
        for (auto it = termsMatchedCount.begin(); it != termsMatchedCount.end(); ++it) {
            if (it.value() == terms.size()) {
                hasAllMatched = true;
                break;
            }
        }

        results.reserve(combinedScores.size());
        for (auto it = combinedScores.begin(); it != combinedScores.end(); ++it) {
            int idx = it.key();
            int matched = termsMatchedCount.value(idx, 0);

            // If any emojis matched all terms, filter out partial matches
            if (hasAllMatched && matched < terms.size()) {
                continue;
            }

            int score = it.value();
            if (matched == terms.size()) {
                score += 8000;
            }

            const auto& e = m_emojis[idx];
            QString nameLower = e.name.toLower();

            // Exact full phrase match
            if (nameLower == q) {
                score += 15000;
            } else if (nameLower.startsWith(q)) {
                score += 10000;
            } else if (nameLower.contains(q)) {
                score += 5000;
            }

            // Check phrase in keywords
            for (const QString& kw : e.keywords) {
                if (kw.toLower() == q) {
                    score += 12000;
                    break;
                }
            }

            if (!e.hasSkinTones) score += 400;
            if (e.category != "Kaomoji") score += 300;
            score += std::max(0, 60 - static_cast<int>(e.name.length()));
            results.append({idx, score});
        }
    }

    std::stable_sort(results.begin(), results.end(), [](const Candidate& a, const Candidate& b) {
        return a.score > b.score;
    });

    QVector<int> out;
    out.reserve(results.size());
    for (const auto& c : results) {
        out.append(c.index);
    }

    // Limit cache size to prevent memory bloat
    if (m_searchCache.size() > 256) {
        m_searchCache.clear();
    }
    m_searchCache.insert(q, out);

    return out;
}

QVector<EmojiEntry> EmojiDatabase::search(const QString& query) const
{
    QVector<int> indices = searchIndices(query);
    QVector<EmojiEntry> results;
    results.reserve(indices.size());
    for (int idx : indices) {
        if (idx >= 0 && idx < m_emojis.size()) {
            results.append(m_emojis[idx]);
        }
    }
    return results;
}

const EmojiEntry* EmojiDatabase::findByCodepoints(const QString& codepoints) const
{
    QString norm = codepoints.toUpper().simplified();
    auto it = m_codepointsIndex.constFind(norm);
    if (it != m_codepointsIndex.constEnd()) {
        int idx = it.value();
        if (idx >= 0 && idx < m_emojis.size()) {
            return &m_emojis[idx];
        }
    }

    for (const EmojiEntry& entry : m_emojis) {
        if (entry.codepoints == norm) {
            return &entry;
        }
    }
    return nullptr;
}

const EmojiEntry* EmojiDatabase::findByChar(const QString& emojiChar) const
{
    auto it = m_charIndex.constFind(emojiChar);
    if (it != m_charIndex.constEnd()) {
        int idx = it.value();
        if (idx >= 0 && idx < m_emojis.size()) {
            return &m_emojis[idx];
        }
    }

    // Try alternate variation selector (with or without FE0F)
    QString altChar = emojiChar;
    if (altChar.contains(QChar(0xFE0F))) {
        altChar.remove(QChar(0xFE0F));
    } else {
        altChar.append(QChar(0xFE0F));
    }
    it = m_charIndex.constFind(altChar);
    if (it != m_charIndex.constEnd()) {
        int idx = it.value();
        if (idx >= 0 && idx < m_emojis.size()) {
            return &m_emojis[idx];
        }
    }

    for (const EmojiEntry& entry : m_emojis) {
        if (entry.emojiChar == emojiChar || entry.emojiChar == altChar) {
            return &entry;
        }
    }
    return nullptr;
}

const EmojiEntry* EmojiDatabase::findEmoji(const QString& codeOrChar) const
{
    const EmojiEntry* entry = findByChar(codeOrChar);
    if (entry) return entry;
    return findByCodepoints(codeOrChar);
}

QVector<EmojiEntry> EmojiDatabase::recentEmojis(const QStringList& recentCodes) const
{
    QVector<EmojiEntry> result;
    for (const QString& code : recentCodes) {
        const EmojiEntry* entry = findEmoji(code);
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
        const EmojiEntry* entry = findEmoji(it.key());
        if (entry) {
            result.append(*entry);
        }
    }
    std::sort(result.begin(), result.end(),
        [&frequentCodes](const EmojiEntry& a, const EmojiEntry& b) {
            int freqA = frequentCodes.value(a.emojiChar, frequentCodes.value(a.codepoints, 0)).toInt();
            int freqB = frequentCodes.value(b.emojiChar, frequentCodes.value(b.codepoints, 0)).toInt();
            return freqA > freqB;
        });
    return result;
}

QVector<EmojiEntry> EmojiDatabase::skinToneVariants(const EmojiEntry& base) const
{
    QVector<EmojiEntry> variants;

    static const QStringList toneCodes = {"1F3FB", "1F3FC", "1F3FD", "1F3FE", "1F3FF"};
    QStringList parts = base.codepoints.split(' ', Qt::SkipEmptyParts);
    QStringList baseParts;
    for (const QString& p : parts) {
        if (!toneCodes.contains(p)) {
            baseParts.append(p);
        }
    }
    QString baseCps = baseParts.join(' ');

    // 1. Add neutral/yellow base entry if available
    const EmojiEntry* baseEntry = findByCodepoints(baseCps);
    if (baseEntry) {
        variants.append(*baseEntry);
    }

    // 2. Add all corresponding skin tone variants
    for (const EmojiEntry& entry : m_emojis) {
        if (!entry.hasSkinTones) continue;
        QStringList ep = entry.codepoints.split(' ', Qt::SkipEmptyParts);
        QStringList eb;
        for (const QString& p : ep) {
            if (!toneCodes.contains(p)) {
                eb.append(p);
            }
        }
        if (eb.join(' ') == baseCps && entry.codepoints != baseCps) {
            variants.append(entry);
        }
    }

    return variants;
}

void EmojiDatabase::loadKaomoji()
{
    if (!m_categories.contains("Kaomoji")) {
        m_categories.append("Kaomoji");
    }

    struct KaomojiDef {
        const char* charText;
        const char* name;
        const char* keywords;
    };

    static const KaomojiDef kaomojiList[] = {
        {"(•‿•)", "Happy Face", "happy smile cute joyful face"},
        {"(◕‿◕✿)", "Flower Girl", "flower girl cheerful cute anime happy"},
        {"(╯°□°)╯︵ ┻━┻", "Table Flip", "table flip rage angry smash tableflip"},
        {"┬─┬ノ( º _ ºノ)", "Put Table Back", "calm respect put table back unflip"},
        {"¯\\_(ツ)_/¯", "Shrug", "shrug whatever idk don't know care"},
        {"( ͡° ͜ʖ ͡°)", "Lenny Face", "lenny face smirk mischief naughty eyes"},
        {"(｡♥‿♥｡)", "Heart Eyes", "heart eyes love crush inlove romance"},
        {"ʕ•ᴥ•ʔ", "Little Bear", "bear animal cute kawaii teddy"},
        {"(>_<)", "Troubled", "troubled annoyed frustrated ouch"},
        {"(づ｡◕‿‿◕｡)づ", "Hugs", "hug hugs embrace love cute warm"},
        {"(ﾉ◕ヮ◕)ﾉ*:･ﾟ✧", "Magic Sparkles", "magic sparkle glitter happy cheer excited"},
        {"ಠ_ಠ", "Look of Disapproval", "disapproval stare glare angry judging suspicious"},
        {"(☞ﾟヮﾟ)☞", "Finger Guns", "finger guns you got it nice hey bro"},
        {"(T_T)", "Crying Face", "cry crying tears sad weep upset"},
        {"(¬_¬)", "Side Glance", "glance side eye suspicious unimpressed whatever"},
        {"(づ￣ ³￣)づ", "Blowing Kiss", "kiss kissing love affection sweetheart"},
        {"(ง'̀-'́)ง", "Ready to Fight", "fight boxer punch challenge angry tough"},
        {"(・_・;)", "Nervous Sweat", "nervous sweat sweatdrop anxious worried"},
        {"(・ω・)", "Cat Face", "cat face kitty pet cute mouth"},
        {"(=^･ω･^=)", "Cat with Ears", "cat kitten ears whiskers meow"},
        {"(｡•́︿•̀｡)", "Sad Pout", "sad pout crying lonely hurt"},
        {"(✿◠‿◠)", "Blushing Flower", "blush shy gentle sweet pretty"},
        {"(¬‿¬)", "Smug Grin", "smug grin knowing evil smile"},
        {"(；一_一)", "Doubtful", "doubt skeptical squint suspect annoyed"},
        {"(★‿★)", "Star Struck", "star eyes wow amazing dazzle"},
        {"(⌐■_■)", "Deal With It", "deal with it sunglasses cool boss"},
        {"(°ロ°) !", "Shocked", "shocked gasp surprise startled what"},
        {"(ノಠ益ಠ)ノ彡┻━┻", "Rage Table Flip", "rage flip fury table furious crazy"}
    };

    for (const auto& k : kaomojiList) {
        EmojiEntry entry;
        entry.emojiChar = QString::fromUtf8(k.charText);
        entry.name = QString::fromUtf8(k.name);
        entry.category = "Kaomoji";
        entry.subcategory = "kaomoji";
        entry.codepoints = QString("KAO_%1").arg(qHash(entry.emojiChar), 8, 16, QChar('0')).toUpper();

        QStringList kw = QString::fromUtf8(k.keywords).split(' ', Qt::SkipEmptyParts);
        kw.append(entry.name.toLower().split(' ', Qt::SkipEmptyParts));
        kw.append("kaomoji");
        kw.removeDuplicates();
        entry.keywords = kw;

        int index = m_emojis.size();
        m_emojis.append(entry);
        m_categoryIndex["Kaomoji"].append(index);
        m_codepointsIndex.insert(entry.codepoints, index);
        m_charIndex.insert(entry.emojiChar, index);
        for (const QString& w : kw) {
            m_keywordIndex[w.toLower()].append(index);
        }
    }
}

void EmojiDatabase::loadSymbols()
{
    if (!m_categories.contains("Symbols")) {
        m_categories.append("Symbols");
    }

    QByteArray jsonData;
    QFile resFile(":/data/symbols.json");
    if (resFile.open(QIODevice::ReadOnly)) {
        jsonData = resFile.readAll();
        resFile.close();
    } else {
        QStringList candidateDirs = {
            QDir::homePath() + "/.local/share/emojityper/data",
            "/usr/share/emojityper/data",
            "/usr/local/share/emojityper/data",
            "/app/share/emojityper/data",
            QDir::currentPath() + "/data",
            QCoreApplication::applicationDirPath() + "/data",
            QCoreApplication::applicationDirPath() + "/../share/emojityper/data"
        };
        for (const QString& dataDir : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
            candidateDirs.append(dataDir + "/emojityper/data");
        }
        for (const QString& dir : candidateDirs) {
            QFile f(dir + "/symbols.json");
            if (f.open(QIODevice::ReadOnly)) {
                jsonData = f.readAll();
                f.close();
                break;
            }
        }
    }

    if (jsonData.isEmpty()) {
        qWarning() << "EmojiDatabase: symbols.json not found";
        return;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseErr);
    if (doc.isNull() || !doc.isArray()) {
        qWarning() << "EmojiDatabase: Failed to parse symbols.json:" << parseErr.errorString();
        return;
    }

    QJsonArray arr = doc.array();
    for (const QJsonValue& val : arr) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        EmojiEntry entry;
        entry.emojiChar = obj["char"].toString();
        entry.name = obj["name"].toString();
        entry.category = "Symbols";
        entry.subcategory = obj.value("subcategory").toString("math");
        entry.codepoints = QString("SYM_%1").arg(qHash(entry.emojiChar), 8, 16, QChar('0')).toUpper();

        QStringList kw;
        QJsonArray kwArr = obj["keywords"].toArray();
        for (const QJsonValue& kVal : kwArr) {
            kw.append(kVal.toString());
        }
        kw.append(entry.name.toLower().split(' ', Qt::SkipEmptyParts));
        kw.append("symbol");
        kw.append(entry.subcategory);
        kw.removeDuplicates();
        entry.keywords = kw;

        int index = m_emojis.size();
        m_emojis.append(entry);
        m_categoryIndex["Symbols"].append(index);
        m_codepointsIndex.insert(entry.codepoints, index);
        m_charIndex.insert(entry.emojiChar, index);
        for (const QString& w : kw) {
            m_keywordIndex[w.toLower()].append(index);
        }
    }
}