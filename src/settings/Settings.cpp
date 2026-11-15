/**
 * Settings implementation
 */
#include "Settings.h"
#include <QtCore/QDebug>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QTimer>
#include <QtGui/QKeySequence>
#include <QtCore/QKeyCombination>
#include <algorithm>

Settings::Settings(QObject* parent) : QObject(parent)
{
    applyDefaults();
}

Settings& Settings::instance()
{
    static Settings s_instance;
    return s_instance;
}

QString Settings::settingsFilePath() const
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configDir.isEmpty()) {
        configDir = QDir::homePath() + "/.config";
    }
    QDir dir(configDir + "/EmojiTyper");
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath("settings.json");
}

void Settings::applyDefaults()
{
    m_hotkeyModifiers = 8;  // Super/Win key
    m_hotkeyKey = 0x2e;     // '.' key (XK_period decimal / Qt::Key_Period)
    m_hotkeySequence = "Meta+.";
    m_autostart = false;
    m_theme = "system";
    m_showRecentFirst = true;
    m_defaultSkinTone = 0;
    m_injectionMode = "auto";
    m_typingDelay = 12;
    m_panelScale = 100;
    m_panelPos = QPoint(-1, -1);  // Invalid = auto-position
    m_panelSize = QSize(400, 300);
}

void Settings::resetToDefaults()
{
    applyDefaults();
    save();
    emit hotkeyChanged();
    emit autostartChanged();
    emit themeChanged();
    emit showRecentFirstChanged();
    emit defaultSkinToneChanged(m_defaultSkinTone);
    emit injectionModeChanged(m_injectionMode);
    emit typingDelayChanged(m_typingDelay);
    emit panelScaleChanged(m_panelScale);
}

void Settings::load()
{
    QString path = settingsFilePath();
    QFile file(path);
    if (!file.exists()) {
        qDebug() << "No settings file, using defaults:" << path;
        applyDefaults();
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open settings file:" << path;
        applyDefaults();
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Settings JSON parse error:" << error.errorString();
        applyDefaults();
        return;
    }

    QJsonObject obj = doc.object();

    m_hotkeyModifiers = obj.value("hotkeyModifiers").toInt(8);
    m_hotkeyKey = obj.value("hotkeyKey").toInt(0x2e);
    m_hotkeySequence = obj.value("hotkeySequence").toString();
    if (m_hotkeySequence.isEmpty()) {
        m_hotkeySequence = hotkeySequence();
    }
    m_autostart = obj.value("autostart").toBool(false);
    m_theme = obj.value("theme").toString("system");
    m_showRecentFirst = obj.value("showRecentFirst").toBool(true);
    m_defaultSkinTone = obj.value("defaultSkinTone").toInt(0);
    m_injectionMode = obj.value("injectionMode").toString("auto");
    m_typingDelay = obj.value("typingDelay").toInt(12);
    m_panelScale = obj.value("panelScale").toInt(100);

    // Recent emojis
    QJsonArray recentArr = obj.value("recentEmojis").toArray();
    m_recentEmojis.clear();
    for (const QJsonValue& v : recentArr) {
        if (m_recentEmojis.size() < MAX_RECENT) {
            m_recentEmojis.append(v.toString());
        }
    }

    // Frequent emojis
    QJsonObject freqObj = obj.value("frequentEmojis").toObject();
    m_frequentEmojis.clear();
    for (auto it = freqObj.begin(); it != freqObj.end(); ++it) {
        m_frequentEmojis[it.key()] = it.value().toInt();
    }

    // Panel position/size
    QJsonObject posObj = obj.value("panelPos").toObject();
    if (!posObj.isEmpty()) {
        m_panelPos = QPoint(posObj.value("x").toInt(-1), posObj.value("y").toInt(-1));
    }
    QJsonObject sizeObj = obj.value("panelSize").toObject();
    if (!sizeObj.isEmpty()) {
        m_panelSize = QSize(sizeObj.value("w").toInt(400), sizeObj.value("h").toInt(300));
    }

    qDebug() << "Settings loaded from:" << path;
}

void Settings::save()
{
    QJsonObject obj;

    obj["hotkeyModifiers"] = m_hotkeyModifiers;
    obj["hotkeyKey"] = m_hotkeyKey;
    obj["hotkeySequence"] = hotkeySequence();
    obj["autostart"] = m_autostart;
    obj["theme"] = m_theme;
    obj["showRecentFirst"] = m_showRecentFirst;
    obj["defaultSkinTone"] = m_defaultSkinTone;
    obj["injectionMode"] = m_injectionMode;
    obj["typingDelay"] = m_typingDelay;
    obj["panelScale"] = m_panelScale;

    // Recent emojis
    QJsonArray recentArr;
    for (const QString& emoji : m_recentEmojis) {
        recentArr.append(emoji);
    }
    obj["recentEmojis"] = recentArr;

    // Frequent emojis
    QJsonObject freqObj;
    for (auto it = m_frequentEmojis.begin(); it != m_frequentEmojis.end(); ++it) {
        freqObj[it.key()] = it.value().toInt();
    }
    obj["frequentEmojis"] = freqObj;

    // Panel position/size
    QJsonObject posObj;
    posObj["x"] = m_panelPos.x();
    posObj["y"] = m_panelPos.y();
    obj["panelPos"] = posObj;

    QJsonObject sizeObj;
    sizeObj["w"] = m_panelSize.width();
    sizeObj["h"] = m_panelSize.height();
    obj["panelSize"] = sizeObj;

    QJsonDocument doc(obj);
    QString path = settingsFilePath();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open settings file for writing:" << path;
        return;
    }

    if (m_saveTimer) {
        m_saveTimer->stop();
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "Settings saved to:" << path;
}

void Settings::saveLater()
{
    if (!m_saveTimer) {
        m_saveTimer = new QTimer(this);
        m_saveTimer->setSingleShot(true);
        connect(m_saveTimer, &QTimer::timeout, this, &Settings::save);
    }
    m_saveTimer->start(1000);
}

QString Settings::hotkeySequence() const
{
    if (!m_hotkeySequence.isEmpty()) {
        return m_hotkeySequence;
    }
    QStringList parts;
    if (m_hotkeyModifiers & 0x08) parts << "Meta";
    if (m_hotkeyModifiers & 0x02) parts << "Ctrl";
    if (m_hotkeyModifiers & 0x04) parts << "Alt";
    if (m_hotkeyModifiers & 0x01) parts << "Shift";

    Qt::Key k = static_cast<Qt::Key>(m_hotkeyKey);
    QKeySequence ks(k);
    QString keyStr = ks.toString(QKeySequence::PortableText);
    if (keyStr.isEmpty()) {
        if (m_hotkeyKey == 0x2e) {
            keyStr = ".";
        } else {
            keyStr = QString::number(m_hotkeyKey, 16);
        }
    }
    parts << keyStr;
    return parts.join("+");
}

void Settings::setHotkeySequence(const QString& sequence)
{
    if (sequence.isEmpty()) return;
    if (m_hotkeySequence != sequence) {
        m_hotkeySequence = sequence;

        QString normalized = sequence;
        normalized.replace("Super", "Meta", Qt::CaseInsensitive);
        normalized.replace("Win", "Meta", Qt::CaseInsensitive);

        QKeySequence ks(normalized, QKeySequence::PortableText);
        if (ks.isEmpty()) {
            ks = QKeySequence(normalized, QKeySequence::NativeText);
        }
        if (!ks.isEmpty()) {
            QKeyCombination comb = ks[0];
            Qt::KeyboardModifiers mods = comb.keyboardModifiers();
            int modMask = 0;
            if (mods & Qt::ShiftModifier) modMask |= 1;
            if (mods & Qt::ControlModifier) modMask |= 2;
            if (mods & Qt::AltModifier) modMask |= 4;
            if (mods & Qt::MetaModifier) modMask |= 8;

            m_hotkeyModifiers = modMask;
            m_hotkeyKey = static_cast<int>(comb.key());
        }

        save();
        emit hotkeyChanged();
    }
}

void Settings::setHotkeyModifiers(int modifiers)
{
    if (m_hotkeyModifiers != modifiers) {
        m_hotkeyModifiers = modifiers;
        m_hotkeySequence.clear();
        m_hotkeySequence = hotkeySequence();
        save();
        emit hotkeyChanged();
    }
}

void Settings::setHotkeyKey(int key)
{
    if (m_hotkeyKey != key) {
        m_hotkeyKey = key;
        m_hotkeySequence.clear();
        m_hotkeySequence = hotkeySequence();
        save();
        emit hotkeyChanged();
    }
}

void Settings::setAutostart(bool enabled)
{
    if (m_autostart != enabled) {
        m_autostart = enabled;

        // Create/remove autostart .desktop file
        QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
        QDir dir(autostartDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        QString desktopFile = autostartDir + "/org.emojiboard.EmojiTyper.desktop";
        QString legacyDesktopFile = autostartDir + "/EmojiTyper.desktop";

        if (enabled) {
            QString exePath = QCoreApplication::applicationFilePath();
            QString content = QString(
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=EmojiTyper\n"
                "GenericName=Emoji Picker\n"
                "Comment=Direct emoji typing tool for Linux (X11 & Wayland)\n"
                "Exec=%1\n"
                "Icon=emojityper\n"
                "Terminal=false\n"
                "Categories=Utility;Accessibility;Qt;\n"
                "Keywords=emoji;emoticon;picker;symbols;unicode;typing;\n"
                "StartupNotify=false\n"
                "X-GNOME-Autostart-enabled=true\n"
                "X-KDE-autostart-after=panel\n"
                "X-KDE-GlobalAccel-Target=true\n"
                "X-KDE-Shortcuts=Meta+.\n"
            ).arg(exePath);
            QFile file(desktopFile);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(content.toUtf8());
                file.close();
            }
        } else {
            QFile::remove(desktopFile);
            QFile::remove(legacyDesktopFile);
        }

        save();
        emit autostartChanged();
    }
}

void Settings::setTheme(const QString& theme)
{
    if (m_theme != theme) {
        m_theme = theme;
        save();
        emit themeChanged();
    }
}

void Settings::setShowRecentFirst(bool show)
{
    if (m_showRecentFirst != show) {
        m_showRecentFirst = show;
        save();
        emit showRecentFirstChanged();
    }
}

void Settings::setDefaultSkinTone(int tone)
{
    tone = std::clamp(tone, 0, 5);
    if (m_defaultSkinTone != tone) {
        m_defaultSkinTone = tone;
        save();
        emit defaultSkinToneChanged(m_defaultSkinTone);
    }
}

void Settings::setInjectionMode(const QString& mode)
{
    if (m_injectionMode != mode) {
        m_injectionMode = mode;
        save();
        emit injectionModeChanged(m_injectionMode);
    }
}

void Settings::setTypingDelay(int delayMs)
{
    delayMs = std::clamp(delayMs, 5, 50);
    if (m_typingDelay != delayMs) {
        m_typingDelay = delayMs;
        save();
        emit typingDelayChanged(m_typingDelay);
    }
}

void Settings::setPanelScale(int percent)
{
    percent = std::clamp(percent, 75, 200);
    if (m_panelScale != percent) {
        m_panelScale = percent;
        save();
        emit panelScaleChanged(m_panelScale);
    }
}

void Settings::addRecentEmoji(const QString& emoji)
{
    if (emoji.isEmpty()) return;

    m_recentEmojis.removeAll(emoji);
    m_recentEmojis.prepend(emoji);

    while (m_recentEmojis.size() > MAX_RECENT) {
        m_recentEmojis.removeLast();
    }

    saveLater();
    emit recentEmojisChanged();
}

void Settings::removeRecentEmoji(const QString& emoji)
{
    if (m_recentEmojis.removeAll(emoji) > 0) {
        saveLater();
        emit recentEmojisChanged();
    }
}

void Settings::clearRecentEmojis()
{
    if (!m_recentEmojis.isEmpty()) {
        m_recentEmojis.clear();
        saveLater();
        emit recentEmojisChanged();
    }
}

void Settings::incrementFrequency(const QString& emoji)
{
    if (emoji.isEmpty()) return;

    int count = m_frequentEmojis.value(emoji, 0).toInt() + 1;
    m_frequentEmojis[emoji] = count;

    if (m_frequentEmojis.size() > MAX_FREQUENT) {
        QList<QPair<QString, int>> sorted;
        for (auto it = m_frequentEmojis.begin(); it != m_frequentEmojis.end(); ++it) {
            sorted.append({it.key(), it.value().toInt()});
        }
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

        while (m_frequentEmojis.size() > MAX_FREQUENT && !sorted.isEmpty()) {
            m_frequentEmojis.remove(sorted.takeFirst().first);
        }
    }

    saveLater();
    emit frequentEmojisChanged();
}

void Settings::removeFrequentEmoji(const QString& emoji)
{
    if (m_frequentEmojis.remove(emoji) > 0) {
        saveLater();
        emit frequentEmojisChanged();
    }
}

void Settings::clearFrequentEmojis()
{
    if (!m_frequentEmojis.isEmpty()) {
        m_frequentEmojis.clear();
        saveLater();
        emit frequentEmojisChanged();
    }
}

void Settings::setPanelPos(const QPoint& pos)
{
    if (m_panelPos != pos) {
        m_panelPos = pos;
        saveLater();
    }
}

void Settings::setPanelSize(const QSize& size)
{
    if (m_panelSize != size) {
        m_panelSize = size;
        saveLater();
    }
}
