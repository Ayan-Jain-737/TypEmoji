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

    loadKaomoji();
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
                entry.keywords.append(tag);
            }
        }
    }

    if (base != entry.codepoints) {
        auto itFull = purposeMap.constFind(entry.codepoints);
        if (itFull != purposeMap.constEnd()) {
            for (const QString& tag : itFull.value()) {
                if (!entry.keywords.contains(tag, Qt::CaseInsensitive)) {
                    entry.keywords.append(tag);
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

QVector<int> EmojiDatabase::searchIndices(const QString& query) const
{
    QString q = query.toLower().trimmed();
    if (q.isEmpty()) return {};

    QStringList terms = q.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (terms.isEmpty()) return {};

    struct MatchCandidate {
        int index;
        int score;
    };
    QVector<MatchCandidate> candidates;
    candidates.reserve(m_emojis.size());

    for (int i = 0; i < m_emojis.size(); ++i) {
        const EmojiEntry& entry = m_emojis[i];
        QString nameLower = entry.name.toLower();
        QStringList nameWords = nameLower.split(QRegularExpression("[\\s,:_\\-]+"), Qt::SkipEmptyParts);

        int totalScore = 0;
        bool allTermsMatched = true;

        for (const QString& term : terms) {
            int termScore = 0;

            // 1. Exact full name match
            if (nameLower == term) {
                termScore = std::max(termScore, 1000);
            }
            // 2. Name starts with whole word term (e.g. "fire engine" for "fire")
            else if (nameLower.startsWith(term + " ") || nameLower.startsWith(term + ":")) {
                int score = 650 + std::max(0, 50 - static_cast<int>(nameLower.length() - term.length()));
                termScore = std::max(termScore, score);
            }

            // 3. Exact word match in name
            for (int wIdx = 0; wIdx < nameWords.size(); ++wIdx) {
                const QString& w = nameWords[wIdx];
                if (w == term) {
                    int score = 550 + (wIdx == 0 ? 100 : 0);
                    termScore = std::max(termScore, score);
                } else if (w.startsWith(term)) {
                    int score = 300 + (wIdx == 0 ? 50 : 0);
                    termScore = std::max(termScore, score);
                }
            }

            // 4. Keywords & Purpose matches
            for (const QString& kw : entry.keywords) {
                QString kwLower = kw.toLower();
                if (kwLower == term) {
                    termScore = std::max(termScore, 520);
                } else if (kwLower.startsWith(term)) {
                    termScore = std::max(termScore, 260);
                } else if (kwLower.contains(term)) {
                    termScore = std::max(termScore, 100);
                }
            }

            // 5. Substring match in name
            if (nameLower.contains(term)) {
                int pos = static_cast<int>(nameLower.indexOf(term));
                int score = 180 - std::min(pos * 5, 80);
                termScore = std::max(termScore, score);
            }

            // 6. Subcategory / Category match
            if (entry.subcategory.toLower().contains(term) || entry.category.toLower().contains(term)) {
                termScore = std::max(termScore, 80);
            }

            // 7. Subsequence match in name (if term length >= 3)
            if (termScore == 0 && term.length() >= 3) {
                qsizetype tIdx = 0;
                for (qsizetype cIdx = 0; cIdx < nameLower.length() && tIdx < term.length(); ++cIdx) {
                    if (nameLower[cIdx] == term[tIdx]) {
                        ++tIdx;
                    }
                }
                if (tIdx == term.length()) {
                    termScore = std::max(termScore, 40);
                }
            }

            if (termScore == 0) {
                allTermsMatched = false;
                break;
            }

            totalScore += termScore;
        }

        if (!allTermsMatched || totalScore <= 0) {
            continue;
        }

        // Base emoji bonus: yellow / neutral default without skin tone gets priority
        if (!entry.hasSkinTones) {
            totalScore += 100;
        }

        // Shorter name brevity bonus
        totalScore += std::max(0, 40 - static_cast<int>(nameLower.length()));

        candidates.append({i, totalScore});
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const MatchCandidate& a, const MatchCandidate& b) {
        return a.score > b.score;
    });

    QVector<int> result;
    result.reserve(candidates.size());
    for (const auto& c : candidates) {
        result.append(c.index);
    }
    return result;
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