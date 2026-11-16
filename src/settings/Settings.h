/**
 * Settings - Persistent application settings (JSON in ~/.config)
 */
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
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
    Q_PROPERTY(QString hotkeySequence READ hotkeySequence WRITE setHotkeySequence NOTIFY hotkeyChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool showRecentFirst READ showRecentFirst WRITE setShowRecentFirst NOTIFY showRecentFirstChanged)
    Q_PROPERTY(int defaultSkinTone READ defaultSkinTone WRITE setDefaultSkinTone NOTIFY defaultSkinToneChanged)
    Q_PROPERTY(QString injectionMode READ injectionMode WRITE setInjectionMode NOTIFY injectionModeChanged)
    Q_PROPERTY(int typingDelay READ typingDelay WRITE setTypingDelay NOTIFY typingDelayChanged)
    Q_PROPERTY(int panelScale READ panelScale WRITE setPanelScale NOTIFY panelScaleChanged)
    Q_PROPERTY(int gridDensity READ gridDensity WRITE setGridDensity NOTIFY gridDensityChanged)

public:
    static Settings& instance();

    ~Settings() override = default;

    // Load settings from disk
    void load();

    // Save settings to disk
    void save();

    // Reset settings to factory defaults
    void resetToDefaults();

    // Hotkey (modifiers: bitmask for Shift=1, Ctrl=2, Alt=4, Super=8)
    int hotkeyModifiers() const { return m_hotkeyModifiers; }
    void setHotkeyModifiers(int modifiers);

    int hotkeyKey() const { return m_hotkeyKey; }
    void setHotkeyKey(int key);

    // Hotkey as human-readable string (e.g. "Meta+.", "Ctrl+Shift+Space")
    QString hotkeySequence() const;
    void setHotkeySequence(const QString& sequence);

    // Autostart with desktop session
    bool autostart() const { return m_autostart; }
    void setAutostart(bool enabled);

    // Theme (system, dark, light, black)
    QString theme() const { return m_theme; }
    void setTheme(const QString& theme);

    // Show recent emojis first
    bool showRecentFirst() const { return m_showRecentFirst; }
    void setShowRecentFirst(bool show);

    // Default skin tone: 0 (default/yellow), 1 (light), 2 (medium-light), 3 (medium), 4 (medium-dark), 5 (dark)
    int defaultSkinTone() const { return m_defaultSkinTone; }
    void setDefaultSkinTone(int tone);

    // Injection mode: "auto", "ydotool", "wtype", "clipboard"
    QString injectionMode() const { return m_injectionMode; }
    void setInjectionMode(const QString& mode);

    // Typing delay / debounce in milliseconds (5ms - 50ms)
    int typingDelay() const { return m_typingDelay; }
    void setTypingDelay(int delayMs);

    // Panel UI scaling percentage (75% - 200%, default 100%)
    int panelScale() const { return m_panelScale; }
    void setPanelScale(int percent);

    // Grid density: 0 = Compact (36px), 1 = Standard (44px), 2 = Large (52px)
    int gridDensity() const { return m_gridDensity; }
    void setGridDensity(int density);

    // History Export / Import
    bool exportHistory(const QString& filePath) const;
    bool importHistory(const QString& filePath);

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
    void defaultSkinToneChanged(int tone);
    void injectionModeChanged(const QString& mode);
    void typingDelayChanged(int delayMs);
    void panelScaleChanged(int scalePercent);
    void gridDensityChanged(int density);

private:
    explicit Settings(QObject* parent = nullptr);

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    // Defaults
    int m_hotkeyModifiers = 8;          // Super/Win key
    int m_hotkeyKey = 0x2e;             // . key (XK_period decimal)
    QString m_hotkeySequence = "Meta+.";
    bool m_autostart = false;
    QString m_theme = "system";          // system, dark, light, black
    bool m_showRecentFirst = true;
    int m_defaultSkinTone = 0;           // 0: default, 1..5: skin tones
    QString m_injectionMode = "auto";    // auto, ydotool, wtype, clipboard
    int m_typingDelay = 12;              // 12ms debounce
    int m_panelScale = 100;              // 100%
    int m_gridDensity = 1;               // 0: Compact (36px), 1: Standard (44px), 2: Large (52px)

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
