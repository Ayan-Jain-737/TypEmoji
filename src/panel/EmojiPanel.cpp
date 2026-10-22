/**
 * EmojiPanel implementation - Modern Windows-style emoji picker panel
 */
#include "EmojiPanel.h"
#include "EmojiSearchProxy.h"
#include "Settings.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QListView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QStackedWidget>
#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>
#include <QtCore/QSettings>
#include <QtCore/QDebug>
#include <QtGui/QCloseEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QFocusEvent>
#include <QtGui/QScreen>
#include <QtWidgets/QStyledItemDelegate>
#include <QtGui/QPainter>

class EmojiDelegate : public QStyledItemDelegate {
public:
    explicit EmojiDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        QString emoji = index.data(Qt::UserRole + 6).toString();
        if (!emoji.isEmpty()) {
            painter->save();
            painter->setRenderHint(QPainter::TextAntialiasing);

            QRect r = opt.rect.adjusted(3, 3, -3, -3);

            // Draw selection background / border
            if (option.state & QStyle::State_Selected) {
                painter->setPen(QPen(QColor(0, 120, 212), 2));
                painter->setBrush(QColor(0, 120, 212, 50));
                painter->drawRoundedRect(r, 8, 8);
            } else if (option.state & QStyle::State_MouseOver) {
                painter->setPen(QPen(QColor(128, 128, 128, 60), 1));
                painter->setBrush(QColor(128, 128, 128, 30));
                painter->drawRoundedRect(r, 8, 8);
            }

            QFont f = opt.font;
            f.setPointSize(28);
            f.setFamilies({"Segoe UI Emoji","Noto Color Emoji","Apple Color Emoji","Symbola","DejaVu Sans"});
            painter->setFont(f);
            painter->setPen(opt.palette.text().color());
            painter->drawText(opt.rect, Qt::AlignCenter, emoji);
            painter->restore();
            return;
        }
        QStyledItemDelegate::paint(painter, option, index);
    }
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(64, 64);
    }
};

EmojiPanel::EmojiPanel(EmojiDatabase* database, EmojiInjector* injector,
                       TargetState* targetState, QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_database(database)
    , m_injector(injector)
    , m_targetState(targetState)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowTitle("EmojiTyper");

    // Load geometry from settings
    Settings& settings = Settings::instance();
    QPoint savedPos = settings.panelPos();
    if (savedPos.x() >= 0 && savedPos.y() >= 0) {
        move(savedPos);
    }
    QSize savedSize = settings.panelSize();
    if (savedSize.width() > 0 && savedSize.height() > 0) {
        resize(savedSize);
    }

    setFocusPolicy(Qt::StrongFocus);

    m_clickOutsideTimer = new QTimer(this);
    m_clickOutsideTimer->setInterval(80);
    connect(m_clickOutsideTimer, &QTimer::timeout, this, [this]() {
        if (!isVisible() || m_isInjecting || !m_wasActivated) return;
        if (!isActiveWindow()) {
            hidePanel();
            return;
        }
        if (QGuiApplication::mouseButtons() != Qt::NoButton) {
            if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
                hidePanel();
            }
        }
    });

    setupUi();
    applyTheme();

    if (qApp) {
        qApp->installEventFilter(this);
    }
}

void EmojiPanel::setupUi()
{
    // Main layout with no margins for edge-to-edge design
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Container widget with rounded corners and shadow
    m_containerWidget = new QWidget(this);
    m_containerWidget->setObjectName("containerWidget");
    QVBoxLayout* containerLayout = new QVBoxLayout(m_containerWidget);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    // Header with search and close button
    m_headerWidget = new QWidget(m_containerWidget);
    m_headerWidget->setObjectName("headerWidget");
    m_headerWidget->setFixedHeight(48);
    QHBoxLayout* headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(12, 8, 12, 8);
    headerLayout->setSpacing(8);

    // Search icon label
    QLabel* searchIcon = new QLabel(m_headerWidget);
    searchIcon->setText("🔍");
    searchIcon->setFixedSize(20, 20);
    searchIcon->setAlignment(Qt::AlignCenter);

    m_searchEdit = new QLineEdit(m_headerWidget);
    m_searchEdit->setPlaceholderText("Search");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedHeight(32);
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setStyleSheet("#searchEdit { border-radius: 16px; }");
    m_searchEdit->setFocusPolicy(Qt::StrongFocus);
    m_searchEdit->installEventFilter(this);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        if (m_proxyModel->rowCount() > 0) {
            QModelIndex firstIdx = m_proxyModel->index(0, 0);
            if (firstIdx.isValid()) {
                onEmojiClicked(firstIdx);
            }
        }
    });

    // Close button
    m_closeBtn = new QPushButton("×", m_headerWidget);
    m_closeBtn->setFixedSize(28, 28);
    m_closeBtn->setObjectName("closeBtn");
    m_closeBtn->setToolTip("Close (Esc)");
    m_closeBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_closeBtn, &QPushButton::clicked, this, &EmojiPanel::hidePanel);

    headerLayout->addWidget(searchIcon);
    headerLayout->addWidget(m_searchEdit, 1);
    headerLayout->addWidget(m_closeBtn);

    // Category tab bar (Windows-style emoji category tabs)
    m_categoryTabBar = new QTabBar(m_containerWidget);
    m_categoryTabBar->setObjectName("categoryTabBar");
    m_categoryTabBar->setExpanding(true);
    m_categoryTabBar->setDrawBase(false);
    m_categoryTabBar->setElideMode(Qt::ElideRight);
    m_categoryTabBar->setUsesScrollButtons(true);
    m_categoryTabBar->setMovable(false);
    m_categoryTabBar->setFocusPolicy(Qt::NoFocus);
    m_categoryTabBar->installEventFilter(this);
    connect(m_categoryTabBar, &QTabBar::currentChanged, this, &EmojiPanel::onCategoryChanged);

    // Stacked widget for category pages
    m_stackedWidget = new QStackedWidget(m_containerWidget);
    m_stackedWidget->setFocusPolicy(Qt::NoFocus);
    m_stackedWidget->setObjectName("stackedWidget");

    // Populate categories
    populateCategories();

    // Model
    m_model = new EmojiModel(m_database, this);
    m_proxyModel = new EmojiSearchProxy(this);
    m_proxyModel->setSourceModel(m_model);

    // Create list views for each category
    createCategoryViews();

    // Footer with preview label
    m_footerWidget = new QWidget(m_containerWidget);
    m_footerWidget->setObjectName("footerWidget");
    m_footerWidget->setFixedHeight(32);
    QHBoxLayout* footerLayout = new QHBoxLayout(m_footerWidget);
    footerLayout->setContentsMargins(16, 4, 16, 4);
    footerLayout->setSpacing(8);
    m_previewLabel = new QLabel(m_footerWidget);
    m_previewLabel->setObjectName("previewLabel");
    m_previewLabel->setText("Select an emoji");
    footerLayout->addWidget(m_previewLabel, 1);

    // Assemble
    containerLayout->addWidget(m_headerWidget);
    containerLayout->addWidget(m_categoryTabBar);
    containerLayout->addWidget(m_stackedWidget, 1);
    containerLayout->addWidget(m_footerWidget);

    m_mainLayout->addWidget(m_containerWidget);

    setMinimumSize(440, 520);
    setMaximumSize(560, 720);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &EmojiPanel::onSearchTextChanged);
}

void EmojiPanel::populateCategories()
{
    // Remove existing tabs
    while (m_categoryTabBar->count() > 0) {
        m_categoryTabBar->removeTab(0);
    }

    // Standard Unicode Emoji category order with Recently used as the first tab.
    m_categoryDefs.clear();
    m_categoryDefs = {
        {"🕒", "Recently used",        "Recent"},
        {"😀", "Smileys & Emotion",     "Smileys & Emotion"},
        {"👋", "People & Body",         "People & Body"},
        {"🐶", "Animals & Nature",      "Animals & Nature"},
        {"🍕", "Food & Drink",          "Food & Drink"},
        {"🚗", "Travel & Places",       "Travel & Places"},
        {"⚽", "Activities",            "Activities"},
        {"💡", "Objects",               "Objects"},
        {"🔣", "Symbols",              "Symbols"},
        {"🏁", "Flags",                "Flags"},
        {"🌐", "All emojis",           "All"}
    };

    for (int i = 0; i < static_cast<int>(m_categoryDefs.size()); ++i) {
        m_categoryTabBar->addTab(QString::fromUtf8(m_categoryDefs[i].emoji));
        m_categoryTabBar->setTabToolTip(i, m_categoryDefs[i].tooltip);
    }
}

void EmojiPanel::createCategoryViews()
{
    // Create one list view per tab
    for (int i = 0; i < static_cast<int>(m_categoryDefs.size()); ++i) {
        QListView* listView = new QListView(m_stackedWidget);
        listView->setModel(m_proxyModel);
        listView->setViewMode(QListView::IconMode);
        listView->setIconSize(QSize(44, 44));
        listView->setGridSize(QSize(68, 68));
        listView->setSpacing(6);
        listView->setResizeMode(QListView::Adjust);
        listView->setFlow(QListView::LeftToRight);
        listView->setWrapping(true);
        listView->setSelectionMode(QAbstractItemView::SingleSelection);
        listView->setFocusPolicy(Qt::StrongFocus);
        listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        listView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        listView->setUniformItemSizes(true);
        listView->setMouseTracking(true);
        listView->setObjectName(QString("listView_%1").arg(i));
        QFont f = listView->font();
        f.setPointSize(28);
        f.setFamilies({"Segoe UI Emoji","Noto Color Emoji","Apple Color Emoji","Symbola","DejaVu Sans"});
        listView->setFont(f);
        listView->setStyleSheet("QListView { outline: none; border: none; } QListView::item { padding: 4px; }");
        listView->setItemDelegate(new EmojiDelegate(listView));
        listView->installEventFilter(this);
        connect(listView, &QListView::clicked, this, &EmojiPanel::onEmojiClicked);

        // Preview on hover / selection
        connect(listView, &QListView::entered, this, [this](const QModelIndex& idx) {
            if (idx.isValid() && m_previewLabel) {
                QString name = idx.data(EmojiModel::NameRole).toString();
                QString emojiChar = idx.data(EmojiModel::EmojiCharRole).toString();
                m_previewLabel->setText(QString("%1  %2").arg(emojiChar, name));
            }
        });
        if (listView->selectionModel()) {
            connect(listView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current, const QModelIndex&) {
                if (current.isValid() && m_previewLabel) {
                    QString name = current.data(EmojiModel::NameRole).toString();
                    QString emojiChar = current.data(EmojiModel::EmojiCharRole).toString();
                    m_previewLabel->setText(QString("%1  %2").arg(emojiChar, name));
                }
            });
        }

        m_listViews.append(listView);
        m_stackedWidget->addWidget(listView);
    }

    // Default start tab: Smileys & Emotion (index 1) or Recent (index 0)
    m_categoryTabBar->setCurrentIndex(1);
    m_stackedWidget->setCurrentIndex(1);
}

void EmojiPanel::applyTheme()
{
    Settings& settings = Settings::instance();
    QString theme = settings.theme();

    if (theme == "dark" || (theme == "system" && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark)) {
        setStyleSheet(R"(
            EmojiPanel {
                background-color: transparent;
            }
            #containerWidget {
                background-color: #1f1f1f;
                border-radius: 12px;
                border: 1px solid #333;
            }
            #headerWidget {
                background-color: transparent;
            }
            #searchEdit {
                background-color: #2d2d2d;
                color: #ffffff;
                border: 1px solid #444;
                border-radius: 20px;
                padding: 0 16px;
                font-size: 14px;
                selection-background-color: #0078d4;
            }
            #searchEdit:focus {
                border: 1px solid #0078d4;
            }
            #closeBtn {
                background-color: transparent;
                color: #cccccc;
                border: none;
                border-radius: 16px;
                font-size: 14px;
                font-weight: bold;
            }
            #closeBtn:hover {
                background-color: #333;
                color: #ffffff;
            }
            #categoryTabBar {
                background-color: transparent;
                border: none;
            }
            #categoryTabBar::tab {
                background-color: transparent;
                color: #aaaaaa;
                border: none;
                border-radius: 8px;
                padding: 8px 12px;
                margin: 2px;
                font-size: 20px;
                min-width: 40px;
                max-width: 48px;
            }
            #categoryTabBar::tab:selected {
                background-color: #2d2d2d;
                color: #ffffff;
            }
            #categoryTabBar::tab:hover:!selected {
                background-color: #333;
                color: #cccccc;
            }
            #stackedWidget {
                background-color: transparent;
                border: none;
            }
            QListView {
                background-color: transparent;
                alternate-background-color: transparent;
                border: none;
                outline: none;
                padding: 8px;
            }
            QListView::item {
                background-color: transparent;
                border-radius: 8px;
                padding: 4px;
            }
            QListView::item:hover {
                background-color: #333;
            }
            QListView::item:selected {
                background-color: #0078d4;
                color: #ffffff;
            }
            QScrollBar:vertical {
                background-color: transparent;
                width: 8px;
                border: none;
            }
            QScrollBar::handle:vertical {
                background-color: #444;
                border-radius: 4px;
                min-height: 30px;
            }
            QScrollBar::handle:vertical:hover {
                background-color: #666;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0;
            }
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
                background: none;
            }
            #footerWidget {
                background-color: #1a1a1a;
                border-top: 1px solid #2d2d2d;
                border-bottom-left-radius: 12px;
                border-bottom-right-radius: 12px;
            }
            #previewLabel {
                color: #888888;
                font-size: 13px;
                font-weight: 500;
            }
        )");
    } else {
        setStyleSheet(R"(
            EmojiPanel {
                background-color: transparent;
            }
            #containerWidget {
                background-color: #ffffff;
                border-radius: 12px;
                border: 1px solid #e0e0e0;
            }
            #headerWidget {
                background-color: transparent;
            }
            #searchEdit {
                background-color: #f3f3f3;
                color: #000000;
                border: 1px solid #e0e0e0;
                border-radius: 20px;
                padding: 0 16px;
                font-size: 14px;
                selection-background-color: #0078d4;
            }
            #searchEdit:focus {
                border: 1px solid #0078d4;
                background-color: #ffffff;
            }
            #closeBtn {
                background-color: transparent;
                color: #666666;
                border: none;
                border-radius: 16px;
                font-size: 14px;
                font-weight: bold;
            }
            #closeBtn:hover {
                background-color: #f0f0f0;
                color: #000000;
            }
            #categoryTabBar {
                background-color: transparent;
                border: none;
            }
            #categoryTabBar::tab {
                background-color: transparent;
                color: #666666;
                border: none;
                border-radius: 8px;
                padding: 8px 12px;
                margin: 2px;
                font-size: 20px;
                min-width: 40px;
                max-width: 48px;
            }
            #categoryTabBar::tab:selected {
                background-color: #f0f0f0;
                color: #000000;
            }
            #categoryTabBar::tab:hover:!selected {
                background-color: #f5f5f5;
                color: #333333;
            }
            #stackedWidget {
                background-color: transparent;
                border: none;
            }
            QListView {
                background-color: transparent;
                alternate-background-color: transparent;
                border: none;
                outline: none;
                padding: 8px;
            }
            QListView::item {
                background-color: transparent;
                border-radius: 8px;
                padding: 4px;
            }
            QListView::item:hover {
                background-color: #f0f0f0;
            }
            QListView::item:selected {
                background-color: #0078d4;
                color: #ffffff;
            }
            QScrollBar:vertical {
                background-color: transparent;
                width: 8px;
                border: none;
            }
            QScrollBar::handle:vertical {
                background-color: #ccc;
                border-radius: 4px;
                min-height: 30px;
            }
            QScrollBar::handle:vertical:hover {
                background-color: #aaa;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0;
            }
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
                background: none;
            }
            #footerWidget {
                background-color: #f8f8f8;
                border-top: 1px solid #ebebeb;
                border-bottom-left-radius: 12px;
                border-bottom-right-radius: 12px;
            }
            #previewLabel {
                color: #666666;
                font-size: 13px;
                font-weight: 500;
            }
        )");
    }
}

void EmojiPanel::showAt(const QPoint& pos)
{
    qDebug() << "EmojiPanel::showAt called with pos:" << pos;
    if (m_targetState && m_targetState->isValid()) {
        QPoint caretPos = m_targetState->caretScreenPos();
        bool useProvidedPos = (pos.x() >= 0 && pos.y() >= 0);
        if (caretPos.isNull() || useProvidedPos) {
            positionAtCaret(useProvidedPos ? pos : caretPos);
        } else {
            positionAtCaret(caretPos);
        }
    } else {
        // Fallback to cursor position
        QPoint cursorPos = QCursor::pos();
        positionAtCaret(cursorPos);
    }

    // Restore recent from settings
    Settings& settings = Settings::instance();
    QStringList recentCodes = settings.recentEmojis();
    QVector<EmojiEntry> recent = m_database ? m_database->recentEmojis(recentCodes) : QVector<EmojiEntry>();
    m_model->setShowRecent(recent);

    int initialTab = !recent.isEmpty() ? 0 : 1;
    m_categoryTabBar->setCurrentIndex(initialTab);
    onCategoryChanged(initialTab);

    m_wasActivated = false;
    m_isInjecting = false;
    m_hasPendingInjection = false;
    m_pendingIndex = QModelIndex();
    m_searchEdit->clear();

    qDebug() << "EmojiPanel: Calling show()";
    show();
    qDebug() << "EmojiPanel: Calling raise()";
    raise();
    activateWindow();

    if (m_targetState) {
        m_targetState->ensurePanelAbove();
    }

    // Direct keyboard focus to search edit so the user can immediately type
    if (m_searchEdit) {
        m_searchEdit->setFocus();
    }

    int curIdx = m_stackedWidget->currentIndex();
    if (curIdx >= 0 && curIdx < m_listViews.size()) {
        QListView* curList = m_listViews[curIdx];
        if (m_proxyModel->rowCount() > 0) {
            curList->setCurrentIndex(m_proxyModel->index(0, 0));
        }
    }

    if (m_clickOutsideTimer) {
        m_clickOutsideTimer->start();
    }

    // Arm auto-close on window deactivation after window manager finishes mapping
    QTimer::singleShot(200, this, [this]() {
        if (isVisible()) {
            m_wasActivated = true;
        }
    });

    qDebug() << "EmojiPanel shown at" << geometry().topLeft();
}

void EmojiPanel::positionAtCaret(const QPoint& caretPos)
{
    // Position panel near caret, with screen edge clamping
    QRect screen = QGuiApplication::primaryScreen()->geometry();
    QPoint pos = caretPos + QPoint(20, 20);

    // Clamp to screen
    if (pos.x() + width() > screen.right()) {
        pos.setX(screen.right() - width());
    }
    if (pos.y() + height() > screen.bottom()) {
        pos.setY(screen.bottom() - height());
    }
    if (pos.x() < screen.left()) {
        pos.setX(screen.left());
    }
    if (pos.y() < screen.top()) {
        pos.setY(screen.top());
    }

    move(pos);
}

void EmojiPanel::hidePanel()
{
    if (isVisible()) {
        m_wasActivated = false;
        if (m_clickOutsideTimer) {
            m_clickOutsideTimer->stop();
        }

        // Save geometry and flush any pending recents to disk immediately
        Settings& settings = Settings::instance();
        settings.setPanelPos(pos());
        settings.setPanelSize(size());
        settings.save();

        if (m_targetState) {
            m_targetState->clear();
        }

        hide();
        emit panelClosed();
        qDebug() << "EmojiPanel hidden";
    }
}

void EmojiPanel::onEmojiClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;

    if (m_isInjecting) {
        // Queue this emoji so fast clicks are never dropped!
        m_pendingIndex = index;
        return;
    }

    // Get emoji character from model
    QVariant data = m_proxyModel->data(index, EmojiModel::EmojiCharRole);
    QString emoji = data.toString();
    if (emoji.isEmpty()) return;

    // Update recent/frequent tracking (FIFO up to 20)
    Settings& settings = Settings::instance();
    settings.addRecentEmoji(emoji);
    settings.incrementFrequency(emoji);

    // Update recent list in model in real-time
    QStringList recentCodes = settings.recentEmojis();
    QVector<EmojiEntry> recent = m_database ? m_database->recentEmojis(recentCodes) : QVector<EmojiEntry>();
    m_model->setShowRecent(recent);

    // Direct injection into the target application text box.
    // The panel stays open so the user can select multiple emojis in sequence!
    if (m_injector) {
        m_isInjecting = true;
        bool success = m_injector->insertEmoji(emoji, m_targetState);
        if (!success) {
            qWarning() << "Emoji injection failed for:" << emoji;
        } else {
            emit emojiInserted(emoji);
        }

        // Return focus immediately to EmojiPanel so the user can continue multi-typing with zero lag!
        if (isVisible()) {
            if (m_targetState) {
                m_targetState->activateEmojiPanel();
            }
            raise();
            activateWindow();
            int curIdx = m_stackedWidget->currentIndex();
            if (curIdx >= 0 && curIdx < m_listViews.size()) {
                m_listViews[curIdx]->setFocus();
            }
        }
        m_isInjecting = false;

        // Immediately handle any pending injection that was queued during the ~20ms injection
        if (m_hasPendingInjection) {
            m_hasPendingInjection = false;
            int curIdx = m_stackedWidget->currentIndex();
            QListView* curList = (curIdx >= 0 && curIdx < m_listViews.size()) ? m_listViews[curIdx] : nullptr;
            if (curList && curList->currentIndex().isValid()) {
                onEmojiClicked(curList->currentIndex());
            } else if (m_proxyModel->rowCount() > 0) {
                onEmojiClicked(m_proxyModel->index(0, 0));
            }
        } else if (m_pendingIndex.isValid()) {
            QModelIndex nextIdx = m_pendingIndex;
            m_pendingIndex = QModelIndex();
            onEmojiClicked(nextIdx);
        }
    }
}

void EmojiPanel::onSearchTextChanged(const QString& text)
{
    QString query = text.trimmed();
    m_proxyModel->setFilterRegularExpression(QRegularExpression());
    m_model->setSearchQuery(query);

    int curIdx = m_categoryTabBar->currentIndex();
    QListView* curList = (curIdx >= 0 && curIdx < m_listViews.size()) ? m_listViews[curIdx] : nullptr;

    if (!query.isEmpty()) {
        if (m_previewLabel) {
            int count = m_proxyModel->rowCount();
            m_previewLabel->setText(QString("Search results for \"%1\" (%2 found)").arg(query).arg(count));
        }
        if (curList && m_proxyModel->rowCount() > 0) {
            curList->setCurrentIndex(m_proxyModel->index(0, 0));
        }
    } else {
        if (curIdx >= 0 && curIdx < static_cast<int>(m_categoryDefs.size())) {
            const QString& category = m_categoryDefs[curIdx].categoryName;
            if (category == "Recent") {
                Settings& settings = Settings::instance();
                QStringList recentCodes = settings.recentEmojis();
                QVector<EmojiEntry> recent = m_database ? m_database->recentEmojis(recentCodes) : QVector<EmojiEntry>();
                m_model->setShowRecent(recent);
                m_model->setCategoryFilter("Recent");
            } else if (category == "All") {
                m_model->clearFilters();
            } else {
                m_model->setCategoryFilter(category);
            }
            if (m_previewLabel) {
                m_previewLabel->setText(m_categoryDefs[curIdx].tooltip);
            }
        }
        if (curList && m_proxyModel->rowCount() > 0) {
            curList->setCurrentIndex(m_proxyModel->index(0, 0));
        }
    }
}

void EmojiPanel::onCategoryChanged(int index)
{
    if (index < 0 || index >= m_stackedWidget->count()) return;
    if (index >= static_cast<int>(m_categoryDefs.size())) return;

    m_stackedWidget->setCurrentIndex(index);

    if (!m_searchEdit->text().isEmpty()) {
        QSignalBlocker blocker(m_searchEdit);
        m_searchEdit->clear();
    }
    m_model->setSearchQuery(QString());

    const QString& category = m_categoryDefs[index].categoryName;

    if (category == "All") {
        m_model->clearFilters();
    } else if (category == "Recent") {
        Settings& settings = Settings::instance();
        QStringList recentCodes = settings.recentEmojis();
        QVector<EmojiEntry> recent = m_database ? m_database->recentEmojis(recentCodes) : QVector<EmojiEntry>();
        m_model->setShowRecent(recent);
        m_model->setCategoryFilter("Recent");
    } else {
        m_model->setCategoryFilter(category);
    }

    if (index < m_listViews.size()) {
        QListView* listView = m_listViews[index];
        if (m_proxyModel->rowCount() > 0) {
            listView->setCurrentIndex(m_proxyModel->index(0, 0));
        }
        listView->scrollToTop();
    }

    if (m_previewLabel) {
        if (category == "Recent" && m_proxyModel->rowCount() == 0) {
            m_previewLabel->setText("No recently used emojis yet. Pick an emoji to get started!");
        } else {
            m_previewLabel->setText(m_categoryDefs[index].tooltip);
        }
    }
}

void EmojiPanel::onSkinToneRequested(const QModelIndex&)
{
    // TODO: Implement skin tone picker
}

void EmojiPanel::saveGeometry()
{
    Settings& settings = Settings::instance();
    settings.setPanelPos(pos());
    settings.setPanelSize(size());
}

void EmojiPanel::closeEvent(QCloseEvent* event)
{
    hidePanel();
    event->ignore();
}


void EmojiPanel::keyPressEvent(QKeyEvent* event)
{
    // If injection is currently occurring (within that tiny ~25ms window)
    if (m_isInjecting) {
        event->accept(); // CRITICAL: Always accept so it NEVER leaks to the target app!
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && !event->isAutoRepeat()) {
            m_hasPendingInjection = true;
        }
        return;
    }

    // Ignore modifier keys and Ctrl combinations (e.g. Ctrl+V paste simulation from ydotool)
    if (event->key() == Qt::Key_Control ||
        event->key() == Qt::Key_Shift ||
        event->key() == Qt::Key_Alt ||
        event->key() == Qt::Key_Meta ||
        event->key() == Qt::Key_Super_L ||
        event->key() == Qt::Key_Super_R ||
        (event->modifiers() & Qt::ControlModifier)) {
        event->accept();
        return;
    }

    int curIdx = m_stackedWidget->currentIndex();
    QListView* curList = (curIdx >= 0 && curIdx < m_listViews.size()) ? m_listViews[curIdx] : nullptr;

    switch (event->key()) {
    case Qt::Key_Escape:
        event->accept();
        if (m_searchEdit && !m_searchEdit->text().isEmpty()) {
            m_searchEdit->clear();
            if (curList) curList->setFocus();
        } else {
            hidePanel();
        }
        break;

    case Qt::Key_Tab:
        event->accept();
        if (m_searchEdit && m_searchEdit->hasFocus()) {
            m_categoryTabBar->setFocus();
        } else if (m_categoryTabBar->hasFocus()) {
            if (curList) curList->setFocus();
        } else {
            if (m_searchEdit) m_searchEdit->setFocus();
        }
        break;

    case Qt::Key_Backtab:
        event->accept();
        if (m_searchEdit && m_searchEdit->hasFocus()) {
            if (curList) curList->setFocus();
        } else if (m_categoryTabBar->hasFocus()) {
            if (m_searchEdit) m_searchEdit->setFocus();
        } else {
            m_categoryTabBar->setFocus();
        }
        break;

    case Qt::Key_Return:
    case Qt::Key_Enter:
        event->accept(); // Always accept Enter to prevent leaking to underlying chat window!
        if (event->isAutoRepeat()) {
            return; // Ignore key repeat to prevent flooding
        }
        if (curList && curList->currentIndex().isValid()) {
            onEmojiClicked(curList->currentIndex());
        } else if (m_proxyModel->rowCount() > 0) {
            onEmojiClicked(m_proxyModel->index(0, 0));
        }
        break;

    case Qt::Key_Down:
        if (m_searchEdit && m_searchEdit->hasFocus()) {
            if (curList) {
                curList->setFocus();
                if (!curList->currentIndex().isValid() && m_proxyModel->rowCount() > 0) {
                    curList->setCurrentIndex(m_proxyModel->index(0, 0));
                }
            }
            break;
        }
        if (curList) {
            QApplication::sendEvent(curList, event);
        }
        break;

    case Qt::Key_Up:
        if (curList && curList->hasFocus()) {
            if (curList->currentIndex().row() < 6 && m_searchEdit) {
                m_searchEdit->setFocus();
                break;
            }
        }
        if (curList) {
            QApplication::sendEvent(curList, event);
        }
        break;

    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
    case Qt::Key_Home:
    case Qt::Key_End:
        if (curList) {
            if (!curList->hasFocus()) {
                curList->setFocus();
            }
            if (!curList->currentIndex().isValid() && m_proxyModel->rowCount() > 0) {
                curList->setCurrentIndex(m_proxyModel->index(0, 0));
            }
            QApplication::sendEvent(curList, event);
        }
        break;

    default:
        // If user is focused on grid and types printable characters, redirect to search edit
        if (curList && curList->hasFocus() && !event->text().isEmpty() && event->text()[0].isPrint()) {
            if (m_searchEdit) {
                m_searchEdit->setFocus();
                m_searchEdit->setText(m_searchEdit->text() + event->text());
            }
        } else {
            QWidget::keyPressEvent(event);
        }
        break;
    }
}

void EmojiPanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::ActivationChange) {
        if (isActiveWindow()) {
            m_wasActivated = true;
        } else if (m_wasActivated && !m_isInjecting && isVisible()) {
            qDebug() << "EmojiPanel: Lost window activation, closing panel";
            hidePanel();
        }
    }
    QWidget::changeEvent(event);
}

void EmojiPanel::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
}

bool EmojiPanel::eventFilter(QObject* watched, QEvent* event)
{
    // Handle keyboard events globally across child widgets
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        int key = keyEvent->key();
        int curIdx = m_stackedWidget->currentIndex();
        QListView* curList = (curIdx >= 0 && curIdx < m_listViews.size()) ? m_listViews[curIdx] : nullptr;

        // Escape: clear search if non-empty, otherwise close panel
        if (key == Qt::Key_Escape) {
            if (m_searchEdit && !m_searchEdit->text().isEmpty()) {
                m_searchEdit->clear();
                if (curList) curList->setFocus();
            } else {
                hidePanel();
            }
            return true;
        }

        // Return / Enter: Inject emoji
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            if (!keyEvent->isAutoRepeat()) {
                if (curList && curList->currentIndex().isValid()) {
                    onEmojiClicked(curList->currentIndex());
                } else if (m_proxyModel->rowCount() > 0) {
                    onEmojiClicked(m_proxyModel->index(0, 0));
                }
            }
            return true;
        }

        // Down arrow from search edit moves focus into grid
        if (watched == m_searchEdit && key == Qt::Key_Down) {
            if (curList) {
                curList->setFocus();
                if (!curList->currentIndex().isValid() && m_proxyModel->rowCount() > 0) {
                    curList->setCurrentIndex(m_proxyModel->index(0, 0));
                }
            }
            return true;
        }

        // Up arrow from top row of grid moves focus back into search edit
        if (watched != m_searchEdit && key == Qt::Key_Up) {
            if (curList && curList->currentIndex().row() < 6) {
                if (m_searchEdit) {
                    m_searchEdit->setFocus();
                    m_searchEdit->setCursorPosition(m_searchEdit->text().length());
                    return true;
                }
            }
        }

        // Backspace when not in searchEdit -> chop last char from searchEdit
        if (watched != m_searchEdit && key == Qt::Key_Backspace) {
            if (m_searchEdit && !m_searchEdit->text().isEmpty()) {
                QString t = m_searchEdit->text();
                t.chop(1);
                m_searchEdit->setText(t);
                return true;
            }
        }

        // Printable text when focused on grid or tabs -> redirect to searchEdit
        if (watched != m_searchEdit && !keyEvent->text().isEmpty() && keyEvent->text()[0].isPrint()) {
            Qt::KeyboardModifiers mods = keyEvent->modifiers();
            if (!(mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
                if (m_searchEdit) {
                    m_searchEdit->setText(m_searchEdit->text() + keyEvent->text());
                    return true;
                }
            }
        }
    }

    // Detect mouse clicks outside the panel to close it (Plasma/Windows behavior)
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        // Check if the click is outside our panel geometry
        if (isVisible() && !geometry().contains(mouseEvent->globalPosition().toPoint())) {
            // Don't hide if the click is on the tray icon (let tray handle it)
            QWidget* clickedWidget = QApplication::widgetAt(mouseEvent->globalPosition().toPoint());
            if (clickedWidget && !isAncestorOf(clickedWidget) && clickedWidget != this) {
                hidePanel();
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}