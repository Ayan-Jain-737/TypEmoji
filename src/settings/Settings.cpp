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
    m_hotkeyKey = 0x2e;  // '.' key (XK_period decimal)
    m_autostart = false;
    m_theme = "system";
    m_showRecentFirst = true;
    m_panelPos = QPoint(-1, -1);  // Invalid = auto-position
    m_panelSize = QSize(400, 300);
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
    m_autostart = obj.value("autostart").toBool(false);
    m_theme = obj.value("theme").toString("system");
    m_showRecentFirst = obj.value("showRecentFirst").toBool(true);

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
    obj["autostart"] = m_autostart;
    obj["theme"] = m_theme;
    obj["showRecentFirst"] = m_showRecentFirst;

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

void Settings::setHotkeyModifiers(int modifiers)
{
    if (m_hotkeyModifiers != modifiers) {
        m_hotkeyModifiers = modifiers;
        save();
        emit hotkeyChanged();
    }
}

void Settings::setHotkeyKey(int key)
{
    if (m_hotkeyKey != key) {
        m_hotkeyKey = key;
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
        QString desktopFile = autostartDir + "/EmojiTyper.desktop";

        if (enabled) {
            QString exePath = QCoreApplication::applicationFilePath();
            QString content = QString(
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=EmojiTyper\n"
                "Exec=%1\n"
                "Hidden=false\n"
                "NoDisplay=false\n"
                "X-GNOME-Autostart-enabled=true\n"
            ).arg(exePath);
            QFile file(desktopFile);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(content.toUtf8());
                file.close();
            }
        } else {
            QFile::remove(desktopFile);
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