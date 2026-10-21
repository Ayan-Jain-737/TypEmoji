/**
 * EmojiPanel - Floating non-activating emoji picker panel
 */
#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QListView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QStackedWidget>
#include <QtCore/QPoint>
#include <QtCore/QSettings>
#include <QtCore/QVector>
#include <QtGui/QGuiApplication>
#include <QtGui/QStyleHints>
#include "EmojiModel.h"
#include "EmojiDatabase.h"
#include "TargetState.h"
#include "EmojiInjector.h"
#include "EmojiSearchProxy.h"
#include "Settings.h"
#include <vector>
#include <string>

// Category definition for tab-to-category mapping
struct CategoryDef {
    const char* emoji;
    const char* tooltip;
    QString categoryName;
};

class EmojiPanel : public QWidget
{
    Q_OBJECT
public:
    explicit EmojiPanel(EmojiDatabase* database, EmojiInjector* injector,
                        TargetState* targetState, QWidget* parent = nullptr);
    ~EmojiPanel() override = default;

    // Show panel at position (or at caret if pos is invalid)
    void showAt(const QPoint& pos = QPoint(-1, -1));

    // Hide panel
    void hidePanel();

    // Check if panel is visible
    bool isVisible() const { return QWidget::isVisible(); }

signals:
    void panelClosed();
    void emojiInserted(const QString& emoji);

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onEmojiClicked(const QModelIndex& index);
    void onSearchTextChanged(const QString& text);
    void onCategoryChanged(int index);
    void onSkinToneRequested(const QModelIndex& index);
    void saveGeometry();

private:
    void setupUi();
    void applyTheme();
    void positionAtCaret(const QPoint& caretPos);
    void populateCategories();
    void createCategoryViews();

    EmojiDatabase* m_database = nullptr;
    EmojiInjector* m_injector = nullptr;
    TargetState* m_targetState = nullptr;

    EmojiModel* m_model = nullptr;
    EmojiSearchProxy* m_proxyModel = nullptr;
    QVector<QListView*> m_listViews;
    QLineEdit* m_searchEdit = nullptr;
    QTabBar* m_categoryTabBar = nullptr;
    QStackedWidget* m_stackedWidget = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;
    QWidget* m_containerWidget = nullptr;
    QWidget* m_headerWidget = nullptr;
    QPushButton* m_closeBtn = nullptr;

    std::vector<CategoryDef> m_categoryDefs;
    QPoint m_lastCaretPos;
    bool m_isClosing = false;
    bool m_isInjecting = false;
    bool m_hasPendingInjection = false;
    QModelIndex m_pendingIndex;
    bool m_wasActivated = false;
    QTimer* m_clickOutsideTimer = nullptr;
};