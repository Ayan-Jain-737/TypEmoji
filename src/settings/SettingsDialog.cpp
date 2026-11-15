/**
 * SettingsDialog implementation - Comprehensive native Qt6 Preferences GUI Dialog for TypEmoji
 */
#include "SettingsDialog.h"
#include "Settings.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QFrame>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtGui/QDesktopServices>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtGui/QStyleHints>
#include <QtCore/QStandardPaths>
#include <QtCore/QFileInfo>
#include <QtCore/QSysInfo>
#include <QtCore/QUrl>
#include <QtCore/QDebug>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    loadSettings();
}

void SettingsDialog::setupUi()
{
    setWindowTitle(tr("TypEmoji — Preferences"));
    setMinimumSize(600, 560);
    resize(640, 580);

    QIcon appIcon(":/icons/emojityper.png");
    if (appIcon.isNull()) {
        appIcon = QIcon(":/icons/emojityper.svg");
    }
    if (appIcon.isNull()) {
        appIcon = QIcon::fromTheme("emojityper");
    }
    setWindowIcon(appIcon);

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(14);

    // Header banner
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    QLabel* iconLabel = new QLabel(this);
    if (!appIcon.isNull()) {
        iconLabel->setPixmap(appIcon.pixmap(36, 36));
    }
    headerLayout->addWidget(iconLabel);

    QVBoxLayout* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    QLabel* titleLabel = new QLabel(tr("TypEmoji Preferences"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(15);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    QLabel* subtitleLabel = new QLabel(tr("Configure shortcuts, appearance, typing behavior, and skin tones"), this);
    subtitleLabel->setStyleSheet("color: rgba(255, 255, 255, 0.65); font-size: 12px;");

    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(subtitleLabel);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    rootLayout->addLayout(headerLayout);

    // Tab Widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setObjectName("settingsTabWidget");

    m_tabWidget->addTab(createGeneralTab(), tr("⚙ General"));
    m_tabWidget->addTab(createShortcutsTab(), tr("⌨ Shortcuts"));
    m_tabWidget->addTab(createAppearanceTab(), tr("🎨 Appearance"));
    m_tabWidget->addTab(createTypingEngineTab(), tr("⚡ Typing Engine"));
    m_tabWidget->addTab(createAboutTab(), tr("ℹ About"));

    rootLayout->addWidget(m_tabWidget, 1);

    // Bottom Action Bar
    QHBoxLayout* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(10);

    m_resetDefaultsBtn = new QPushButton(tr("Reset All to Defaults"), this);
    m_resetDefaultsBtn->setObjectName("resetDefaultsBtn");
    m_resetDefaultsBtn->setCursor(Qt::PointingHandCursor);
    connect(m_resetDefaultsBtn, &QPushButton::clicked, this, &SettingsDialog::resetToDefaults);
    actionLayout->addWidget(m_resetDefaultsBtn);

    actionLayout->addStretch();

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setObjectName("cancelBtn");
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    actionLayout->addWidget(m_cancelBtn);

    m_applyBtn = new QPushButton(tr("Apply"), this);
    m_applyBtn->setObjectName("applyBtn");
    m_applyBtn->setCursor(Qt::PointingHandCursor);
    connect(m_applyBtn, &QPushButton::clicked, this, &SettingsDialog::apply);
    actionLayout->addWidget(m_applyBtn);

    m_saveBtn = new QPushButton(tr("Save"), this);
    m_saveBtn->setObjectName("saveBtn");
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    m_saveBtn->setDefault(true);
    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsDialog::saveAndClose);
    actionLayout->addWidget(m_saveBtn);

    rootLayout->addLayout(actionLayout);

    applyDialogStyle(Settings::instance().theme());
}

QWidget* SettingsDialog::createGeneralTab()
{
    QWidget* tab = new QWidget(this);
    QVBoxLayout* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(14, 16, 14, 16);
    tabLayout->setSpacing(16);

    // Section 1: Startup & Session
    QGroupBox* sessionGroup = new QGroupBox(tr("Startup & Session"), tab);
    QVBoxLayout* sessionLayout = new QVBoxLayout(sessionGroup);
    sessionLayout->setSpacing(10);

    m_autostartCheck = new QCheckBox(tr("Run TypEmoji on system startup"), sessionGroup);
    m_autostartCheck->setToolTip(tr("Automatically launches TypEmoji daemon when you log in to your desktop session"));
    sessionLayout->addWidget(m_autostartCheck);

    QLabel* autoNote = new QLabel(tr("Creates an XDG autostart entry in ~/.config/autostart"), sessionGroup);
    autoNote->setStyleSheet("color: rgba(255, 255, 255, 0.5); font-size: 11px; margin-left: 24px;");
    sessionLayout->addWidget(autoNote);

    m_showRecentCheck = new QCheckBox(tr("Show Recent tab first on startup"), sessionGroup);
    m_showRecentCheck->setToolTip(tr("Opens to your recently used emojis instead of the Smileys category"));
    sessionLayout->addWidget(m_showRecentCheck);

    QLabel* recentNote = new QLabel(tr("If enabled, your most recent and frequent emojis will be immediately visible"), sessionGroup);
    recentNote->setStyleSheet("color: rgba(255, 255, 255, 0.5); font-size: 11px; margin-left: 24px;");
    sessionLayout->addWidget(recentNote);

    tabLayout->addWidget(sessionGroup);

    // Section 2: Default Skin Tone
    QGroupBox* toneGroup = new QGroupBox(tr("Default Skin Tone"), tab);
    QVBoxLayout* toneLayout = new QVBoxLayout(toneGroup);
    toneLayout->setSpacing(8);

    QLabel* toneDesc = new QLabel(tr("Preferred skin tone applied to person, hand, and body emojis:"), toneGroup);
    toneDesc->setStyleSheet("color: rgba(255, 255, 255, 0.7); font-size: 12px;");
    toneLayout->addWidget(toneDesc);

    QHBoxLayout* pillsLayout = new QHBoxLayout();
    pillsLayout->setSpacing(6);

    m_skinToneGroup = new QButtonGroup(this);
    m_skinToneGroup->setExclusive(true);

    const struct {
        int id;
        const char* label;
        const char* tooltip;
    } tones[] = {
        {0, "🟡 Default", "Default neutral yellow tone"},
        {1, "🏻 Light", "Fitzpatrick Type 1-2"},
        {2, "🏼 Med-Light", "Fitzpatrick Type 3"},
        {3, "🏽 Medium", "Fitzpatrick Type 4"},
        {4, "🏾 Med-Dark", "Fitzpatrick Type 5"},
        {5, "🏿 Dark", "Fitzpatrick Type 6"}
    };

    for (int i = 0; i < 6; ++i) {
        QPushButton* btn = new QPushButton(tones[i].label, toneGroup);
        btn->setCheckable(true);
        btn->setToolTip(tones[i].tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setObjectName(QString("skinToneBtn_%1").arg(tones[i].id));
        btn->setStyleSheet(
            "QPushButton {"
            "  border: 1px solid rgba(255, 255, 255, 0.15);"
            "  border-radius: 14px;"
            "  padding: 6px 10px;"
            "  background-color: rgba(255, 255, 255, 0.06);"
            "  font-size: 12px;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(255, 255, 255, 0.12);"
            "  border-color: rgba(255, 255, 255, 0.25);"
            "}"
            "QPushButton:checked {"
            "  background-color: #0078D4;"
            "  color: #FFFFFF;"
            "  font-weight: bold;"
            "  border-color: #0098FF;"
            "}"
        );
        m_skinToneGroup->addButton(btn, tones[i].id);
        m_skinToneButtons[i] = btn;
        pillsLayout->addWidget(btn);
    }

    connect(m_skinToneGroup, &QButtonGroup::idClicked, this, &SettingsDialog::onSkinToneButtonClicked);

    toneLayout->addLayout(pillsLayout);
    tabLayout->addWidget(toneGroup);

    // Section 3: History & Privacy
    QGroupBox* historyGroup = new QGroupBox(tr("Emoji History & Privacy"), tab);
    QHBoxLayout* historyLayout = new QHBoxLayout(historyGroup);
    historyLayout->setSpacing(12);

    m_historyStatusLabel = new QLabel(historyGroup);
    m_historyStatusLabel->setStyleSheet("color: rgba(255, 255, 255, 0.7); font-size: 12px;");
    historyLayout->addWidget(m_historyStatusLabel, 1);

    m_clearHistoryBtn = new QPushButton(tr("Clear History"), historyGroup);
    m_clearHistoryBtn->setObjectName("clearHistoryBtn");
    m_clearHistoryBtn->setCursor(Qt::PointingHandCursor);
    connect(m_clearHistoryBtn, &QPushButton::clicked, this, &SettingsDialog::onClearHistoryClicked);
    historyLayout->addWidget(m_clearHistoryBtn);

    tabLayout->addWidget(historyGroup);
    tabLayout->addStretch();

    return tab;
}

QWidget* SettingsDialog::createShortcutsTab()
{
    QWidget* tab = new QWidget(this);
    QVBoxLayout* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(14, 16, 14, 16);
    tabLayout->setSpacing(16);

    // Section 1: Global Shortcut Recorder
    QGroupBox* globalGroup = new QGroupBox(tr("Global Activation Shortcut"), tab);
    QVBoxLayout* globalLayout = new QVBoxLayout(globalGroup);
    globalLayout->setSpacing(10);

    QLabel* globalDesc = new QLabel(
        tr("Press this shortcut from any application window to immediately toggle the emoji picker:"),
        globalGroup
    );
    globalDesc->setStyleSheet("color: rgba(255, 255, 255, 0.75); font-size: 12px;");
    globalLayout->addWidget(globalDesc);

    QHBoxLayout* recorderLayout = new QHBoxLayout();
    recorderLayout->setSpacing(10);

    m_keySequenceEdit = new QKeySequenceEdit(globalGroup);
    m_keySequenceEdit->setClearButtonEnabled(true);
    m_keySequenceEdit->setObjectName("keySequenceEdit");
    m_keySequenceEdit->setMinimumHeight(34);
    connect(m_keySequenceEdit, &QKeySequenceEdit::keySequenceChanged, this, &SettingsDialog::onKeySequenceChanged);
    recorderLayout->addWidget(m_keySequenceEdit, 1);

    m_resetShortcutBtn = new QPushButton(tr("Reset (Meta + .)"), globalGroup);
    m_resetShortcutBtn->setObjectName("resetShortcutBtn");
    m_resetShortcutBtn->setCursor(Qt::PointingHandCursor);
    connect(m_resetShortcutBtn, &QPushButton::clicked, this, &SettingsDialog::onResetShortcutClicked);
    recorderLayout->addWidget(m_resetShortcutBtn);

    globalLayout->addLayout(recorderLayout);

    m_currentShortcutBadge = new QLabel(globalGroup);
    m_currentShortcutBadge->setTextFormat(Qt::RichText);
    globalLayout->addWidget(m_currentShortcutBadge);

    QLabel* backendNote = new QLabel(
        tr("Supported backends: KDE KGlobalAccel (Wayland/X11), XDG Desktop Portal (GNOME/Hyprland), X11 XGrabKey."),
        globalGroup
    );
    backendNote->setStyleSheet("color: rgba(255, 255, 255, 0.45); font-size: 11px;");
    globalLayout->addWidget(backendNote);

    tabLayout->addWidget(globalGroup);

    // Section 2: Quick Reference
    QGroupBox* refGroup = new QGroupBox(tr("In-Panel Keyboard Shortcuts"), tab);
    QGridLayout* refLayout = new QGridLayout(refGroup);
    refLayout->setContentsMargins(14, 12, 14, 12);
    refLayout->setHorizontalSpacing(16);
    refLayout->setVerticalSpacing(8);

    const struct {
        const char* key;
        const char* desc;
    } shortcuts[] = {
        {"↵ Enter", "Insert emoji into active application and close picker"},
        {"⇧ Shift + ↵", "Insert emoji while keeping picker open for multiple selections"},
        {"Esc", "Dismiss emoji picker immediately"},
        {"Tab / ⇧ Tab", "Cycle forward / backward across category tabs"},
        {"↑ ↓ ← →", "Navigate smoothly through emoji grid"},
        {"Type text", "Instantly search emojis by name, keyword, or purpose"},
        {"Right-click / Hold", "Open skin tone variants menu for supported emojis"}
    };

    for (int i = 0; i < 7; ++i) {
        QLabel* keyBadge = new QLabel(shortcuts[i].key, refGroup);
        keyBadge->setStyleSheet(
            "background-color: rgba(255, 255, 255, 0.1);"
            "border: 1px solid rgba(255, 255, 255, 0.2);"
            "border-radius: 4px;"
            "padding: 2px 8px;"
            "font-weight: bold;"
            "font-family: monospace;"
            "font-size: 11px;"
        );
        keyBadge->setAlignment(Qt::AlignCenter);

        QLabel* descLabel = new QLabel(shortcuts[i].desc, refGroup);
        descLabel->setStyleSheet("color: rgba(255, 255, 255, 0.75); font-size: 12px;");

        refLayout->addWidget(keyBadge, i, 0);
        refLayout->addWidget(descLabel, i, 1);
    }

    tabLayout->addWidget(refGroup);
    tabLayout->addStretch();

    return tab;
}

QWidget* SettingsDialog::createAppearanceTab()
{
    QWidget* tab = new QWidget(this);
    QVBoxLayout* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(14, 16, 14, 16);
    tabLayout->setSpacing(16);

    // Section 1: Theme Selection
    QGroupBox* themeGroup = new QGroupBox(tr("Color Theme"), tab);
    QVBoxLayout* themeLayout = new QVBoxLayout(themeGroup);
    themeLayout->setSpacing(8);

    m_themeCombo = new QComboBox(themeGroup);
    m_themeCombo->setObjectName("themeCombo");
    m_themeCombo->addItem(tr("System Theme (Automatic)"), "system");
    m_themeCombo->addItem(tr("Dark Mode (Obsidian)"), "dark");
    m_themeCombo->addItem(tr("Light Mode"), "light");
    m_themeCombo->addItem(tr("OLED Pitch Black"), "black");
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::onThemeComboChanged);
    themeLayout->addWidget(m_themeCombo);

    m_themeDescLabel = new QLabel(themeGroup);
    m_themeDescLabel->setStyleSheet("color: rgba(255, 255, 255, 0.65); font-size: 11px;");
    themeLayout->addWidget(m_themeDescLabel);

    tabLayout->addWidget(themeGroup);

    // Section 2: Panel Size & Scaling
    QGroupBox* sizeGroup = new QGroupBox(tr("Panel Dimensions & Scaling"), tab);
    QVBoxLayout* sizeLayout = new QVBoxLayout(sizeGroup);
    sizeLayout->setSpacing(12);

    QHBoxLayout* sizePresetLayout = new QHBoxLayout();
    QLabel* presetLabel = new QLabel(tr("Size Preset:"), sizeGroup);
    presetLabel->setStyleSheet("font-size: 12px;");
    sizePresetLayout->addWidget(presetLabel);

    m_sizePresetCombo = new QComboBox(sizeGroup);
    m_sizePresetCombo->setObjectName("sizePresetCombo");
    m_sizePresetCombo->addItem(tr("Compact (350 × 260 px)"), QSize(350, 260));
    m_sizePresetCombo->addItem(tr("Standard (400 × 300 px) — Default"), QSize(400, 300));
    m_sizePresetCombo->addItem(tr("Large (480 × 360 px)"), QSize(480, 360));
    m_sizePresetCombo->addItem(tr("Extra Large (560 × 420 px)"), QSize(560, 420));
    connect(m_sizePresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        QVariant data = m_sizePresetCombo->itemData(index);
        if (data.isValid()) {
            QSize s = data.toSize();
            Settings::instance().setPanelSize(s);
            if (m_currentSizeLabel) {
                m_currentSizeLabel->setText(tr("Current saved dimensions: %1 × %2 px").arg(s.width()).arg(s.height()));
            }
        }
    });
    sizePresetLayout->addWidget(m_sizePresetCombo, 1);
    sizeLayout->addLayout(sizePresetLayout);

    // Scale slider
    QHBoxLayout* scaleLayout = new QHBoxLayout();
    QLabel* scaleTitle = new QLabel(tr("Interface Scaling:"), sizeGroup);
    scaleTitle->setStyleSheet("font-size: 12px;");
    scaleLayout->addWidget(scaleTitle);

    m_scaleSlider = new QSlider(Qt::Horizontal, sizeGroup);
    m_scaleSlider->setRange(75, 150);
    m_scaleSlider->setSingleStep(5);
    m_scaleSlider->setPageStep(10);
    m_scaleSlider->setValue(100);
    connect(m_scaleSlider, &QSlider::valueChanged, this, &SettingsDialog::onScaleSliderChanged);
    scaleLayout->addWidget(m_scaleSlider, 1);

    m_scaleLabel = new QLabel(tr("100%"), sizeGroup);
    m_scaleLabel->setMinimumWidth(44);
    m_scaleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_scaleLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    scaleLayout->addWidget(m_scaleLabel);
    sizeLayout->addLayout(scaleLayout);

    // Indicator & reset
    QHBoxLayout* indicatorLayout = new QHBoxLayout();
    m_currentSizeLabel = new QLabel(sizeGroup);
    m_currentSizeLabel->setStyleSheet("color: rgba(255, 255, 255, 0.6); font-size: 11px;");
    indicatorLayout->addWidget(m_currentSizeLabel, 1);

    m_resetSizeBtn = new QPushButton(tr("Reset Size & Position"), sizeGroup);
    m_resetSizeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_resetSizeBtn, &QPushButton::clicked, this, &SettingsDialog::onResetSizeClicked);
    indicatorLayout->addWidget(m_resetSizeBtn);
    sizeLayout->addLayout(indicatorLayout);

    tabLayout->addWidget(sizeGroup);
    tabLayout->addStretch();

    return tab;
}

QWidget* SettingsDialog::createTypingEngineTab()
{
    QWidget* tab = new QWidget(this);
    QVBoxLayout* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(14, 16, 14, 16);
    tabLayout->setSpacing(16);

    // Section 1: Injection Method
    QGroupBox* engineGroup = new QGroupBox(tr("Emoji Injection Method"), tab);
    QVBoxLayout* engineLayout = new QVBoxLayout(engineGroup);
    engineLayout->setSpacing(10);

    m_injectionCombo = new QComboBox(engineGroup);
    m_injectionCombo->setObjectName("injectionCombo");
    m_injectionCombo->addItem(tr("Auto-detect (Recommended)"), "auto");
    m_injectionCombo->addItem(tr("Hardware-level (ydotool / uinput)"), "ydotool");
    m_injectionCombo->addItem(tr("Wayland compositor (wtype)"), "wtype");
    m_injectionCombo->addItem(tr("Clipboard Paste Fallback (Ctrl+V)"), "clipboard");
    connect(m_injectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::onInjectionComboChanged);
    engineLayout->addWidget(m_injectionCombo);

    m_injectionDescLabel = new QLabel(engineGroup);
    m_injectionDescLabel->setWordWrap(true);
    m_injectionDescLabel->setStyleSheet("color: rgba(255, 255, 255, 0.7); font-size: 11px; line-height: 1.4;");
    engineLayout->addWidget(m_injectionDescLabel);

    tabLayout->addWidget(engineGroup);

    // Section 2: Keystroke Delay Slider
    QGroupBox* delayGroup = new QGroupBox(tr("Typing Delay & Debounce"), tab);
    QVBoxLayout* delayLayout = new QVBoxLayout(delayGroup);
    delayLayout->setSpacing(8);

    QLabel* delayDesc = new QLabel(
        tr("Pacing between keystroke sequences. Lower values type faster; higher values prevent dropped characters in remote/slow apps:"),
        delayGroup
    );
    delayDesc->setStyleSheet("color: rgba(255, 255, 255, 0.7); font-size: 12px;");
    delayLayout->addWidget(delayDesc);

    QHBoxLayout* sliderLayout = new QHBoxLayout();
    QLabel* fastLabel = new QLabel(tr("5 ms (Fast)"), delayGroup);
    fastLabel->setStyleSheet("color: rgba(255, 255, 255, 0.45); font-size: 11px;");
    sliderLayout->addWidget(fastLabel);

    m_delaySlider = new QSlider(Qt::Horizontal, delayGroup);
    m_delaySlider->setRange(5, 50);
    m_delaySlider->setSingleStep(1);
    m_delaySlider->setPageStep(5);
    m_delaySlider->setValue(12);
    connect(m_delaySlider, &QSlider::valueChanged, this, &SettingsDialog::onDelaySliderChanged);
    sliderLayout->addWidget(m_delaySlider, 1);

    QLabel* safeLabel = new QLabel(tr("50 ms (Safe)"), delayGroup);
    safeLabel->setStyleSheet("color: rgba(255, 255, 255, 0.45); font-size: 11px;");
    sliderLayout->addWidget(safeLabel);

    m_delayLabel = new QLabel(tr("12 ms"), delayGroup);
    m_delayLabel->setMinimumWidth(50);
    m_delayLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_delayLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #0078D4;");
    sliderLayout->addWidget(m_delayLabel);

    delayLayout->addLayout(sliderLayout);
    tabLayout->addWidget(delayGroup);

    // Section 3: Environment Detection
    QGroupBox* envGroup = new QGroupBox(tr("Detected Linux Environment"), tab);
    QVBoxLayout* envLayout = new QVBoxLayout(envGroup);
    envLayout->setSpacing(6);

    QString platform = QGuiApplication::platformName();
    QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    QString session = qEnvironmentVariable("XDG_SESSION_TYPE");

    m_sessionTypeLabel = new QLabel(
        tr("<b>Display Server:</b> %1 (%2) &nbsp;|&nbsp; <b>Desktop:</b> %3")
            .arg(platform.toUpper(), session.isEmpty() ? "native" : session, desktop.isEmpty() ? "Unknown" : desktop),
        envGroup
    );
    m_sessionTypeLabel->setStyleSheet("font-size: 12px; color: rgba(255, 255, 255, 0.85);");
    envLayout->addWidget(m_sessionTypeLabel);

    bool hasYdotool = !QStandardPaths::findExecutable("ydotool").isEmpty();
    bool hasWtype = !QStandardPaths::findExecutable("wtype").isEmpty();
    bool hasWlCopy = !QStandardPaths::findExecutable("wl-copy").isEmpty();
    bool hasXdotool = !QStandardPaths::findExecutable("xdotool").isEmpty();

    QStringList tools;
    tools << QString("ydotool: %1").arg(hasYdotool ? "<span style='color:#4CAF50;'>✓ Available</span>" : "<span style='color:#F44336;'>✗ Not Found</span>");
    tools << QString("wtype: %1").arg(hasWtype ? "<span style='color:#4CAF50;'>✓ Available</span>" : "<span style='color:#F44336;'>✗ Not Found</span>");
    tools << QString("wl-clipboard: %1").arg(hasWlCopy ? "<span style='color:#4CAF50;'>✓ Available</span>" : "<span style='color:#F44336;'>✗ Not Found</span>");
    tools << QString("xdotool: %1").arg(hasXdotool ? "<span style='color:#4CAF50;'>✓ Available</span>" : "<span style='color:#888888;'>- Optional</span>");

    m_toolsStatusLabel = new QLabel(tools.join(" &nbsp;•&nbsp; "), envGroup);
    m_toolsStatusLabel->setTextFormat(Qt::RichText);
    m_toolsStatusLabel->setStyleSheet("font-size: 11px;");
    envLayout->addWidget(m_toolsStatusLabel);

    tabLayout->addWidget(envGroup);
    tabLayout->addStretch();

    return tab;
}

QWidget* SettingsDialog::createAboutTab()
{
    QWidget* tab = new QWidget(this);
    QVBoxLayout* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(18, 20, 18, 16);
    tabLayout->setSpacing(14);

    // App banner
    QHBoxLayout* bannerLayout = new QHBoxLayout();
    bannerLayout->setSpacing(16);

    QLabel* logoLabel = new QLabel(tab);
    QIcon appIcon(":/icons/emojityper.png");
    if (appIcon.isNull()) appIcon = QIcon(":/icons/emojityper.svg");
    if (!appIcon.isNull()) {
        logoLabel->setPixmap(appIcon.pixmap(56, 56));
    }
    bannerLayout->addWidget(logoLabel);

    QVBoxLayout* titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);

    QLabel* appName = new QLabel(tr("TypEmoji"), tab);
    QFont f = appName->font();
    f.setPointSize(18);
    f.setBold(true);
    appName->setFont(f);

    m_versionLabel = new QLabel(tr("Version 2.1.0 • Modern Linux Emoji & Symbol Picker"), tab);
    m_versionLabel->setStyleSheet("color: rgba(255, 255, 255, 0.7); font-size: 12px;");

    titleBox->addWidget(appName);
    titleBox->addWidget(m_versionLabel);
    bannerLayout->addLayout(titleBox);
    bannerLayout->addStretch();

    tabLayout->addLayout(bannerLayout);

    QFrame* hline = new QFrame(tab);
    hline->setFrameShape(QFrame::HLine);
    hline->setStyleSheet("background-color: rgba(255, 255, 255, 0.1); max-height: 1px;");
    tabLayout->addWidget(hline);

    // Details grid
    QGroupBox* infoGroup = new QGroupBox(tr("Project & System Information"), tab);
    QGridLayout* infoLayout = new QGridLayout(infoGroup);
    infoLayout->setContentsMargins(14, 12, 14, 12);
    infoLayout->setHorizontalSpacing(20);
    infoLayout->setVerticalSpacing(8);

    const struct {
        const char* key;
        const char* val;
    } meta[] = {
        {"License:", "MIT License"},
        {"Author:", "Ayan Jain & TypEmoji Contributors"},
        {"Framework:", QT_VERSION_STR " (Qt6 C++23 native)"},
        {"Emoji Standard:", "Unicode 16.0 / CLDR Annotations"},
        {"Source Repository:", "https://github.com/Ayan-Jain-737/TypEmoji"}
    };

    for (int i = 0; i < 5; ++i) {
        QLabel* kLbl = new QLabel(meta[i].key, infoGroup);
        kLbl->setStyleSheet("color: rgba(255, 255, 255, 0.55); font-weight: bold; font-size: 12px;");
        QLabel* vLbl = new QLabel(meta[i].val, infoGroup);
        vLbl->setStyleSheet("color: rgba(255, 255, 255, 0.9); font-size: 12px;");
        if (i == 4) {
            vLbl->setText(QString("<a href='%1' style='color:#0078D4;'>%1</a>").arg(meta[i].val));
            vLbl->setOpenExternalLinks(true);
        }
        infoLayout->addWidget(kLbl, i, 0);
        infoLayout->addWidget(vLbl, i, 1);
    }

    tabLayout->addWidget(infoGroup);

    // Link Buttons
    QHBoxLayout* linksLayout = new QHBoxLayout();
    linksLayout->setSpacing(10);

    QPushButton* ghBtn = new QPushButton(tr("GitHub Repository"), tab);
    ghBtn->setCursor(Qt::PointingHandCursor);
    connect(ghBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/Ayan-Jain-737/TypEmoji"));
    });
    linksLayout->addWidget(ghBtn);

    QPushButton* issuesBtn = new QPushButton(tr("Report an Issue"), tab);
    issuesBtn->setCursor(Qt::PointingHandCursor);
    connect(issuesBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/Ayan-Jain-737/TypEmoji/issues"));
    });
    linksLayout->addWidget(issuesBtn);

    QPushButton* diagBtn = new QPushButton(tr("Copy Diagnostics"), tab);
    diagBtn->setCursor(Qt::PointingHandCursor);
    connect(diagBtn, &QPushButton::clicked, this, &SettingsDialog::onCopyDiagnosticsClicked);
    linksLayout->addWidget(diagBtn);

    tabLayout->addLayout(linksLayout);

    m_diagnosticsFeedbackLabel = new QLabel(tab);
    m_diagnosticsFeedbackLabel->setStyleSheet("color: #4CAF50; font-size: 11px;");
    m_diagnosticsFeedbackLabel->setAlignment(Qt::AlignCenter);
    tabLayout->addWidget(m_diagnosticsFeedbackLabel);

    tabLayout->addStretch();
    return tab;
}

void SettingsDialog::loadSettings()
{
    Settings& settings = Settings::instance();

    // General tab
    if (m_autostartCheck) {
        m_autostartCheck->setChecked(settings.autostart());
    }
    if (m_showRecentCheck) {
        m_showRecentCheck->setChecked(settings.showRecentFirst());
    }

    m_selectedSkinTone = settings.defaultSkinTone();
    if (m_selectedSkinTone >= 0 && m_selectedSkinTone < 6 && m_skinToneButtons[m_selectedSkinTone]) {
        m_skinToneButtons[m_selectedSkinTone]->setChecked(true);
    }
    updateHistoryCountLabel();

    // Shortcuts tab
    m_currentShortcut = settings.hotkeySequence();
    if (m_keySequenceEdit) {
        m_keySequenceEdit->setKeySequence(QKeySequence(m_currentShortcut));
    }
    updateShortcutBadge(m_currentShortcut);

    // Appearance tab
    if (m_themeCombo) {
        int idx = m_themeCombo->findData(settings.theme());
        if (idx >= 0) {
            m_themeCombo->setCurrentIndex(idx);
        }
    }
    if (m_scaleSlider) {
        m_scaleSlider->setValue(settings.panelScale());
    }
    QSize curSize = settings.panelSize();
    if (m_currentSizeLabel) {
        m_currentSizeLabel->setText(tr("Current saved dimensions: %1 × %2 px").arg(curSize.width()).arg(curSize.height()));
    }

    // Typing Engine tab
    if (m_injectionCombo) {
        int idx = m_injectionCombo->findData(settings.injectionMode());
        if (idx >= 0) {
            m_injectionCombo->setCurrentIndex(idx);
        }
    }
    if (m_delaySlider) {
        m_delaySlider->setValue(settings.typingDelay());
    }
}

void SettingsDialog::apply()
{
    Settings& settings = Settings::instance();

    // General
    if (m_autostartCheck) {
        settings.setAutostart(m_autostartCheck->isChecked());
    }
    if (m_showRecentCheck) {
        settings.setShowRecentFirst(m_showRecentCheck->isChecked());
    }
    settings.setDefaultSkinTone(m_selectedSkinTone);

    // Shortcuts
    if (m_keySequenceEdit) {
        QKeySequence ks = m_keySequenceEdit->keySequence();
        if (!ks.isEmpty()) {
            settings.setHotkeySequence(ks.toString(QKeySequence::PortableText));
        }
    }

    // Appearance
    if (m_themeCombo) {
        QString theme = m_themeCombo->currentData().toString();
        settings.setTheme(theme);
        applyDialogStyle(theme);
    }
    if (m_scaleSlider) {
        settings.setPanelScale(m_scaleSlider->value());
    }

    // Typing engine
    if (m_injectionCombo) {
        settings.setInjectionMode(m_injectionCombo->currentData().toString());
    }
    if (m_delaySlider) {
        settings.setTypingDelay(m_delaySlider->value());
    }

    settings.save();
}

void SettingsDialog::saveAndClose()
{
    apply();
    accept();
}

void SettingsDialog::resetToDefaults()
{
    int ret = QMessageBox::question(
        this,
        tr("Reset All Preferences"),
        tr("Are you sure you want to reset all preferences to their factory defaults?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (ret == QMessageBox::Yes) {
        Settings::instance().resetToDefaults();
        loadSettings();
    }
}

void SettingsDialog::showAboutTab()
{
    if (m_tabWidget) {
        m_tabWidget->setCurrentIndex(4);
    }
}

void SettingsDialog::onThemeComboChanged(int index)
{
    if (index < 0 || !m_themeCombo) return;
    QString theme = m_themeCombo->itemData(index).toString();

    if (m_themeDescLabel) {
        if (theme == "system") {
            m_themeDescLabel->setText(tr("Automatically detects and matches your desktop environment light/dark theme preference."));
        } else if (theme == "dark") {
            m_themeDescLabel->setText(tr("Deep gray Obsidian dark aesthetic (#202124), gentle on the eyes with high contrast."));
        } else if (theme == "light") {
            m_themeDescLabel->setText(tr("Clean, bright white palette (#FFFFFF) with crisp borders and optimal daytime legibility."));
        } else if (theme == "black") {
            m_themeDescLabel->setText(tr("Pure pitch black (#000000) for OLED screens, saving power with maximum contrast."));
        }
    }
}

void SettingsDialog::onInjectionComboChanged(int index)
{
    if (index < 0 || !m_injectionCombo) return;
    QString mode = m_injectionCombo->itemData(index).toString();

    if (m_injectionDescLabel) {
        if (mode == "auto") {
            m_injectionDescLabel->setText(tr("<b>Auto-detect:</b> Probes your display server and chooses the fastest reliable mechanism (XTest on X11, wtype on wlroots, ydotool on Wayland, with seamless clipboard paste fallback)."));
        } else if (mode == "ydotool") {
            m_injectionDescLabel->setText(tr("<b>ydotool / uinput:</b> Injects keystrokes directly into Linux kernel input devices. Works across all Wayland compositors (GNOME, KDE Plasma, Sway, Hyprland). Requires ydotoold daemon."));
        } else if (mode == "wtype") {
            m_injectionDescLabel->setText(tr("<b>wtype:</b> Direct virtual-keyboard protocol injection for Wayland. Low latency and reliable on wlroots compositors."));
        } else if (mode == "clipboard") {
            m_injectionDescLabel->setText(tr("<b>Clipboard Paste Fallback:</b> Copies emoji to clipboard, simulates Ctrl+V / Shift+Insert, and safely restores prior clipboard contents. Works universally on all systems."));
        }
    }
}

void SettingsDialog::onDelaySliderChanged(int value)
{
    if (m_delayLabel) {
        m_delayLabel->setText(tr("%1 ms").arg(value));
    }
}

void SettingsDialog::onScaleSliderChanged(int value)
{
    if (m_scaleLabel) {
        m_scaleLabel->setText(tr("%1%").arg(value));
    }
}

void SettingsDialog::onKeySequenceChanged(const QKeySequence& sequence)
{
    QString seqStr = sequence.toString(QKeySequence::PortableText);
    m_currentShortcut = seqStr;
    updateShortcutBadge(seqStr);
}

void SettingsDialog::onResetShortcutClicked()
{
    m_currentShortcut = "Meta+.";
    if (m_keySequenceEdit) {
        m_keySequenceEdit->setKeySequence(QKeySequence("Meta+."));
    }
    updateShortcutBadge("Meta+.");
}

void SettingsDialog::updateShortcutBadge(const QString& shortcutText)
{
    if (!m_currentShortcutBadge) return;

    if (shortcutText.isEmpty()) {
        m_currentShortcutBadge->setText(tr("<span style='color:#FF9800;'>⚠ No shortcut assigned</span>"));
        return;
    }

    QStringList keys = shortcutText.split('+', Qt::SkipEmptyParts);
    QStringList kbdList;
    for (const QString& k : keys) {
        QString display = k.trimmed();
        if (display.compare("Meta", Qt::CaseInsensitive) == 0) display = "Super / Win";
        kbdList << QString("<kbd style='background-color:rgba(255,255,255,0.12); border:1px solid rgba(255,255,255,0.25); border-radius:5px; padding:3px 8px; font-weight:bold; font-family:monospace;'>%1</kbd>").arg(display);
    }

    m_currentShortcutBadge->setText(
        tr("Current shortcut: &nbsp;%1").arg(kbdList.join(" &nbsp;+&nbsp; "))
    );
}

void SettingsDialog::onClearHistoryClicked()
{
    int ret = QMessageBox::question(
        this,
        tr("Clear Emoji History"),
        tr("This will remove all stored recent and frequent emoji records from disk. Proceed?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (ret == QMessageBox::Yes) {
        Settings::instance().clearRecentEmojis();
        Settings::instance().clearFrequentEmojis();
        updateHistoryCountLabel();
        QMessageBox::information(this, tr("History Cleared"), tr("Recent and frequent emoji history has been cleared."));
    }
}

void SettingsDialog::onResetSizeClicked()
{
    Settings::instance().setPanelSize(QSize(400, 300));
    Settings::instance().setPanelPos(QPoint(-1, -1));
    if (m_currentSizeLabel) {
        m_currentSizeLabel->setText(tr("Current saved dimensions: 400 × 300 px (Auto-centered)"));
    }
    if (m_sizePresetCombo) {
        m_sizePresetCombo->setCurrentIndex(1);
    }
}

void SettingsDialog::onSkinToneButtonClicked(int id)
{
    m_selectedSkinTone = id;
}

void SettingsDialog::updateHistoryCountLabel()
{
    if (!m_historyStatusLabel) return;
    Settings& settings = Settings::instance();
    int recentCount = settings.recentEmojis().size();
    int freqCount = settings.frequentEmojis().size();
    m_historyStatusLabel->setText(
        tr("History Status: <b>%1</b> recent emojis stored • <b>%2</b> frequent records tracked")
            .arg(recentCount).arg(freqCount)
    );
}

void SettingsDialog::onCopyDiagnosticsClicked()
{
    Settings& settings = Settings::instance();
    QString diag = QString(
        "### TypEmoji System Diagnostics\n"
        "- **Version:** 2.1.0\n"
        "- **Qt Version:** %1\n"
        "- **Platform:** %2\n"
        "- **Session Type:** %3\n"
        "- **Desktop:** %4\n"
        "- **Kernel:** %5\n"
        "- **Configured Shortcut:** %6\n"
        "- **Injection Mode:** %7\n"
        "- **Theme:** %8\n"
        "- **Typing Delay:** %9 ms\n"
        "- **Default Skin Tone:** %10\n"
    ).arg(
        QT_VERSION_STR,
        QGuiApplication::platformName(),
        qEnvironmentVariable("XDG_SESSION_TYPE"),
        qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
        QSysInfo::kernelType() + " " + QSysInfo::kernelVersion(),
        settings.hotkeySequence(),
        settings.injectionMode(),
        settings.theme(),
        QString::number(settings.typingDelay()),
        QString::number(settings.defaultSkinTone())
    );

    QClipboard* cb = QGuiApplication::clipboard();
    if (cb) {
        cb->setText(diag);
        if (m_diagnosticsFeedbackLabel) {
            m_diagnosticsFeedbackLabel->setText(tr("✓ Diagnostic details copied to clipboard!"));
        }
    }
}

void SettingsDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange) {
        applyDialogStyle(Settings::instance().theme());
    }
    QDialog::changeEvent(event);
}

void SettingsDialog::applyDialogStyle(const QString& theme)
{
    bool isDark = (theme == "dark" || theme == "black");
    if (theme == "system") {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        isDark = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
        isDark = (palette().color(QPalette::Window).lightness() < 128);
#endif
    }

    QString bg = isDark ? (theme == "black" ? "#000000" : "#202124") : "#F8F9FA";
    QString cardBg = isDark ? (theme == "black" ? "#121212" : "#2A2A2E") : "#FFFFFF";
    QString textColor = isDark ? "#F2F2F4" : "#1A1A1A";
    QString borderColor = isDark ? "rgba(255, 255, 255, 0.1)" : "rgba(0, 0, 0, 0.12)";
    QString tabBg = isDark ? "rgba(255, 255, 255, 0.05)" : "rgba(0, 0, 0, 0.04)";
    QString tabSelected = isDark ? "#2D2E32" : "#E8EAED";
    QString accent = "#0078D4";

    QString qss = QString(R"(
        SettingsDialog {
            background-color: %1;
            color: %2;
        }
        QTabWidget::pane {
            border: 1px solid %3;
            border-radius: 10px;
            background-color: %1;
            padding: 4px;
        }
        QTabBar::tab {
            background-color: %4;
            color: %2;
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 7px 14px;
            margin-right: 4px;
            font-size: 12px;
            font-weight: 500;
        }
        QTabBar::tab:selected {
            background-color: %5;
            border-bottom: 2px solid %6;
            color: %6;
            font-weight: bold;
        }
        QTabBar::tab:hover:!selected {
            background-color: rgba(255, 255, 255, 0.08);
        }
        QGroupBox {
            background-color: %7;
            border: 1px solid %3;
            border-radius: 10px;
            margin-top: 10px;
            padding-top: 14px;
            font-weight: bold;
            font-size: 12px;
            color: %2;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 14px;
            padding: 0 6px;
        }
        QComboBox, QKeySequenceEdit {
            background-color: rgba(255, 255, 255, 0.07);
            border: 1px solid %3;
            border-radius: 6px;
            padding: 5px 10px;
            color: %2;
            font-size: 12px;
        }
        QComboBox:hover, QKeySequenceEdit:hover {
            border-color: %6;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QSlider::groove:horizontal {
            border: none;
            height: 6px;
            background: rgba(255, 255, 255, 0.15);
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: %6;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #FFFFFF;
            border: 1px solid rgba(0, 0, 0, 0.2);
            width: 16px;
            margin-top: -5px;
            margin-bottom: -5px;
            border-radius: 8px;
        }
        QCheckBox {
            font-size: 13px;
            color: %2;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 1px solid %3;
            border-radius: 4px;
            background-color: rgba(255, 255, 255, 0.05);
        }
        QCheckBox::indicator:checked {
            background-color: %6;
            border-color: %6;
        }
        QPushButton#saveBtn {
            background-color: %6;
            color: #FFFFFF;
            border: none;
            border-radius: 6px;
            padding: 7px 20px;
            font-weight: bold;
            font-size: 12px;
        }
        QPushButton#saveBtn:hover {
            background-color: #1084D9;
        }
        QPushButton#applyBtn, QPushButton#cancelBtn, QPushButton#resetDefaultsBtn {
            background-color: rgba(255, 255, 255, 0.08);
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 7px 16px;
            font-size: 12px;
        }
        QPushButton#applyBtn:hover, QPushButton#cancelBtn:hover, QPushButton#resetDefaultsBtn:hover {
            background-color: rgba(255, 255, 255, 0.15);
        }
    )")
    .arg(bg)
    .arg(textColor)
    .arg(borderColor)
    .arg(tabBg)
    .arg(tabSelected)
    .arg(accent)
    .arg(cardBg);

    setStyleSheet(qss);
}
