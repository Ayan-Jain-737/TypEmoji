/**
 * SettingsDialog - Comprehensive native Qt6 Preferences GUI Dialog for TypEmoji
 */
#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSlider>
#include <QtWidgets/QLabel>
#include <QtWidgets/QKeySequenceEdit>
#include <QtCore/QString>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    // Load current settings from Settings model into UI controls
    void loadSettings();

    // Switch directly to the About tab
    void showAboutTab();

public slots:
    void apply();
    void saveAndClose();
    void resetToDefaults();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onThemeComboChanged(int index);
    void onInjectionComboChanged(int index);
    void onDelaySliderChanged(int value);
    void onScaleSliderChanged(int value);
    void onKeySequenceChanged(const QKeySequence& sequence);
    void onResetShortcutClicked();
    void onClearHistoryClicked();
    void onResetSizeClicked();
    void onSkinToneButtonClicked(int id);
    void onCopyDiagnosticsClicked();

private:
    void setupUi();
    QWidget* createGeneralTab();
    QWidget* createShortcutsTab();
    QWidget* createAppearanceTab();
    QWidget* createTypingEngineTab();
    QWidget* createAboutTab();
    void applyDialogStyle(const QString& theme);
    void updateShortcutBadge(const QString& shortcutText);
    void updateHistoryCountLabel();

    // Tab widget
    QTabWidget* m_tabWidget = nullptr;

    // General Tab widgets
    QCheckBox* m_autostartCheck = nullptr;
    QCheckBox* m_showRecentCheck = nullptr;
    QButtonGroup* m_skinToneGroup = nullptr;
    QPushButton* m_skinToneButtons[6] = {nullptr};
    int m_selectedSkinTone = 0;
    QLabel* m_historyStatusLabel = nullptr;
    QPushButton* m_clearHistoryBtn = nullptr;

    // Shortcuts Tab widgets
    QKeySequenceEdit* m_keySequenceEdit = nullptr;
    QPushButton* m_resetShortcutBtn = nullptr;
    QLabel* m_currentShortcutBadge = nullptr;
    QString m_currentShortcut;

    // Appearance Tab widgets
    QComboBox* m_themeCombo = nullptr;
    QLabel* m_themeDescLabel = nullptr;
    QComboBox* m_sizePresetCombo = nullptr;
    QSlider* m_scaleSlider = nullptr;
    QLabel* m_scaleLabel = nullptr;
    QLabel* m_currentSizeLabel = nullptr;
    QPushButton* m_resetSizeBtn = nullptr;

    // Typing Engine Tab widgets
    QComboBox* m_injectionCombo = nullptr;
    QLabel* m_injectionDescLabel = nullptr;
    QSlider* m_delaySlider = nullptr;
    QLabel* m_delayLabel = nullptr;
    QLabel* m_sessionTypeLabel = nullptr;
    QLabel* m_toolsStatusLabel = nullptr;

    // About Tab widgets
    QLabel* m_versionLabel = nullptr;
    QLabel* m_diagnosticsFeedbackLabel = nullptr;

    // Dialog action buttons
    QPushButton* m_resetDefaultsBtn = nullptr;
    QPushButton* m_applyBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
};
