/**
 * EmojiPanel implementation - Modern Windows-style emoji picker panel
 */
#include "EmojiPanel.h"
#include "EmojiSearchProxy.h"
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
        // Use EmojiCharRole (Qt::UserRole + 6) instead of DisplayRole
        QString emoji = index.data(Qt::UserRole + 6).toString();
        if (!emoji.isEmpty()) {
            painter->save();
            painter->setRenderHint(QPainter::TextAntialiasing);
            QFont f = opt.font;
            f.setPointSize(28);
            f.setFamilies({"Segoe UI Emoji","Noto Color Emoji","Apple Color Emoji","Symbola","DejaVu Sans"});
            painter->setFont(f);
            QRect r = opt.rect.adjusted(4,4,-4,-4);
            painter->drawText(r, Qt::AlignCenter, emoji);
            if (option.state & QStyle::State_Selected) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor(0,120,212,40));
                painter->drawRoundedRect(option.rect, 8,8);
            }
            painter->restore();
            return;
        }
        QStyledItemDelegate::paint(painter, option, index);
    }
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(64,64);
    }
};

EmojiPanel::EmojiPanel(EmojiDatabase* database, EmojiInjector* injector,
                       TargetState* targetState, QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus | Qt::WindowStaysOnTopHint)
    , m_database(database)
    , m_injector(injector)
    , m_targetState(targetState)
{
    setAttribute(Qt::WA_ShowWithoutActivating, true);
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

    setFocusPolicy(Qt::NoFocus);

    m_clickOutsideTimer = new QTimer(this);
    m_clickOutsideTimer->setInterval(50);
    connect(m_clickOutsideTimer, &QTimer::timeout, this, [this]() {
        if (!isVisible()) return;
        if (m_isInjecting) return;
        if (QGuiApplication::mouseButtons() != Qt::NoButton) {
            if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
                hidePanel();
            }
        }
    });

    setupUi();
    applyTheme();

    // No auto-hide timer. Panel stays open until user dismisses it
    // (Esc, close button, or clicking outside) — matching Plasma/Windows behavior.

    // Install global event filter to detect clicks outside the panel
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
    m_searchEdit->setFocusPolicy(Qt::ClickFocus);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        int curIdx = m_stackedWidget->currentIndex();
        if (m_listViews.value(curIdx) && m_proxyModel->rowCount() > 0) {
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

    // Assemble
    containerLayout->addWidget(m_headerWidget);
    containerLayout->addWidget(m_categoryTabBar);
    containerLayout->addWidget(m_stackedWidget, 1);

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

    // Standard Unicode Emoji category order (matches emoji-test.txt group order).
    // Each tab maps to a specific category name from the database.
    // This order is hardcoded to match the standard and avoid depending on
    // alphabetical sort order from EmojiDatabase::categories().
    m_categoryDefs.clear();
    m_categoryDefs = {
        {"🔍", "All emojis",           "All"},
        {"😀", "Smileys & Emotion",     "Smileys & Emotion"},
        {"👋", "People & Body",         "People & Body"},
        {"🐶", "Animals & Nature",      "Animals & Nature"},
        {"🍕", "Food & Drink",          "Food & Drink"},
        {"🚗", "Travel & Places",       "Travel & Places"},
        {"⚽", "Activities",            "Activities"},
        {"💡", "Objects",               "Objects"},
        {"🔣", "Symbols",              "Symbols"},
        {"🏁", "Flags",                "Flags"},
        {"⏱",  "Recent & Frequent",    "Recent"}
    };

    for (int i = 0; i < static_cast<int>(m_categoryDefs.size()); ++i) {
        m_categoryTabBar->addTab(QString::fromUtf8(m_categoryDefs[i].emoji));
        m_categoryTabBar->setTabToolTip(i, m_categoryDefs[i].tooltip);
    }
}

void EmojiPanel::createCategoryViews()
{
    // Create one list view per tab (matching m_categoryDefs from populateCategories).
    // All views share the same model — category filtering happens in onCategoryChanged().
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
        listView->setFocusPolicy(Qt::NoFocus);
        listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        listView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        listView->setUniformItemSizes(true);
        listView->setObjectName(QString("listView_%1").arg(i));
        QFont f = listView->font();
        f.setPointSize(28);
        f.setFamilies({"Segoe UI Emoji","Noto Color Emoji","Apple Color Emoji","Symbola","DejaVu Sans"});
        listView->setFont(f);
        listView->setStyleSheet("QListView { outline: none; border: none; } QListView::item { padding: 6px; }");
        listView->setItemDelegate(new EmojiDelegate(listView));
        connect(listView, &QListView::clicked, this, &EmojiPanel::onEmojiClicked);
        m_listViews.append(listView);
        m_stackedWidget->addWidget(listView);
    }

    // Start on "All" tab (index 0)
    m_categoryTabBar->setCurrentIndex(0);
    m_stackedWidget->setCurrentIndex(0);
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

    // Restore recent/frequent from settings
    Settings& settings = Settings::instance();
    QStringList recentCodes = settings.recentEmojis();
    QVector<EmojiEntry> recent = m_database ? m_database->recentEmojis(recentCodes) : QVector<EmojiEntry>();
    m_model->setShowRecent(recent);

    QVariantMap frequentMap = settings.frequentEmojis();
    QVector<EmojiEntry> frequent = m_database ? m_database->frequentEmojis(frequentMap) : QVector<EmojiEntry>();
    m_model->setShowFrequent(frequent);

    qDebug() << "EmojiPanel: Calling show()";
    show();
    qDebug() << "EmojiPanel: Calling raise()";
    raise();
    // Don't call activateWindow() - panel must remain non-activating per SPEC R2

    if (m_clickOutsideTimer) {
        m_clickOutsideTimer->start();
    }

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
        if (m_clickOutsideTimer) {
            m_clickOutsideTimer->stop();
        }

        // Save geometry
        Settings& settings = Settings::instance();
        settings.setPanelPos(pos());
        settings.setPanelSize(size());

        hide();
        emit panelClosed();
        qDebug() << "EmojiPanel hidden";
    }
}

void EmojiPanel::onEmojiClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;

    // Get emoji character from model
    QVariant data = m_proxyModel->data(index, EmojiModel::EmojiCharRole);
    QString emoji = data.toString();
    if (emoji.isEmpty()) return;

    // Update recent/frequent tracking
    Settings& settings = Settings::instance();
    settings.addRecentEmoji(emoji);
    settings.incrementFrequency(emoji);

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
        QTimer::singleShot(300, this, [this]() {
            m_isInjecting = false;
        });
    }
}

void EmojiPanel::onSearchTextChanged(const QString& text)
{
    m_proxyModel->setFilterRegularExpression(QRegularExpression(text, QRegularExpression::CaseInsensitiveOption));
    // Switch to "All" category when searching
    if (!text.isEmpty()) {
        m_categoryTabBar->setCurrentIndex(0);
        m_stackedWidget->setCurrentIndex(0);
    }
}

void EmojiPanel::onCategoryChanged(int index)
{
    if (index < 0 || index >= m_stackedWidget->count()) return;
    if (index >= static_cast<int>(m_categoryDefs.size())) return;

    m_stackedWidget->setCurrentIndex(index);

    // Look up the category name directly from m_categoryDefs — no reliance on
    // database ordering. This guarantees tab→category mapping is always correct.
    const QString& category = m_categoryDefs[index].categoryName;

    if (category == "All") {
        m_model->clearFilters();
    } else if (category == "Recent") {
        Settings& settings = Settings::instance();
        QStringList recentCodes = settings.recentEmojis();
        QVector<EmojiEntry> recent = m_database ? m_database->recentEmojis(recentCodes) : QVector<EmojiEntry>();
        m_model->setShowRecent(recent);

        QVariantMap frequentMap = settings.frequentEmojis();
        QVector<EmojiEntry> frequent = m_database ? m_database->frequentEmojis(frequentMap) : QVector<EmojiEntry>();
        m_model->setShowFrequent(frequent);
        m_model->showOnlySpecialSections();
    } else {
        m_model->setCategoryFilter(category);
    }
    m_searchEdit->clear();
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
    // Ignore any simulated key events from injection
    if (m_isInjecting) {
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
        return;
    }

    switch (event->key()) {
    case Qt::Key_Escape:
        hidePanel();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_listViews.value(m_stackedWidget->currentIndex()) &&
            m_listViews[m_stackedWidget->currentIndex()]->currentIndex().isValid()) {
            onEmojiClicked(m_listViews[m_stackedWidget->currentIndex()]->currentIndex());
        }
        break;
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        QWidget::keyPressEvent(event);
        break;
    default:
        // If searchEdit does not have focus, any other key closes the panel
        // so the user can immediately resume typing normal text
        if (m_searchEdit && !m_searchEdit->hasFocus()) {
            hidePanel();
            event->ignore();
        } else {
            QWidget::keyPressEvent(event);
        }
        break;
    }
}

void EmojiPanel::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
}

bool EmojiPanel::eventFilter(QObject* watched, QEvent* event)
{
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