/**
 * Settings - Persistent application settings (JSON in ~/.config)
 */
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QJsonObject>
#include <QtCore/QVariantMap>
#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtCore/QCoreApplication>

class Settings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int hotkeyModifiers READ hotkeyModifiers WRITE setHotkeyModifiers NOTIFY hotkeyChanged)
    Q_PROPERTY(int hotkeyKey READ hotkeyKey WRITE setHotkeyKey NOTIFY hotkeyChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool showRecentFirst READ showRecentFirst WRITE setShowRecentFirst NOTIFY showRecentFirstChanged)

public:
    static Settings& instance();

    ~Settings() override = default;

    // Load settings from disk
    void load();

    // Save settings to disk
    void save();

    // Hotkey (modifiers: bitmask for Shift=1, Ctrl=2, Alt=4, Super=8)
    int hotkeyModifiers() const { return m_hotkeyModifiers; }
    void setHotkeyModifiers(int modifiers);

    int hotkeyKey() const { return m_hotkeyKey; }
    void setHotkeyKey(int key);

    // Autostart with desktop session
    bool autostart() const { return m_autostart; }
    void setAutostart(bool enabled);

    // Theme
    QString theme() const { return m_theme; }
    void setTheme(const QString& theme);

    // Show recent emojis first
    bool showRecentFirst() const { return m_showRecentFirst; }
    void setShowRecentFirst(bool show);

    // Recent emojis (max 50)
    QStringList recentEmojis() const { return m_recentEmojis; }
    void addRecentEmoji(const QString& emoji);
    void removeRecentEmoji(const QString& emoji);
    void clearRecentEmojis();

    // Frequent emojis (frequency tracking)
    QVariantMap frequentEmojis() const { return m_frequentEmojis; }
    void incrementFrequency(const QString& emoji);
    void removeFrequentEmoji(const QString& emoji);
    void clearFrequentEmojis();

    // Panel position/size
    QPoint panelPos() const { return m_panelPos; }
    void setPanelPos(const QPoint& pos);

    QSize panelSize() const { return m_panelSize; }
    void setPanelSize(const QSize& size);

signals:
    void hotkeyChanged();
    void autostartChanged();
    void themeChanged();
    void showRecentFirstChanged();
    void recentEmojisChanged();
    void frequentEmojisChanged();

private:
    explicit Settings(QObject* parent = nullptr);

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    // Defaults
        int m_hotkeyModifiers = 8;      // Super/Win key
        int m_hotkeyKey = 0x2e;          // . key (XK_period decimal)
    bool m_autostart = false;
    QString m_theme = "system";          // system, light, dark
    bool m_showRecentFirst = true;

    QStringList m_recentEmojis;
    QVariantMap m_frequentEmojis;        // emoji -> count
    QPoint m_panelPos;
    QSize m_panelSize;

    static const int MAX_RECENT = 20;
    static const int MAX_FREQUENT = 100;

    class QTimer* m_saveTimer = nullptr;
    void saveLater();
    QString settingsFilePath() const;
    void applyDefaults();
};