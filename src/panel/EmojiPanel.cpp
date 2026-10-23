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
#include <QtWidgets/QMenu>
#include <QtWidgets/QWidgetAction>
#include <QtGui/QPainter>

class EmojiDelegate : public QStyledItemDelegate {
public:
    explicit EmojiDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        QString emoji = index.data(EmojiModel::EmojiCharRole).toString();
        if (!emoji.isEmpty()) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setRenderHint(QPainter::TextAntialiasing);

            QRect r = opt.rect.adjusted(1, 1, -1, -1);

            // Draw selection background / border
            if (option.state & QStyle::State_Selected) {
                painter->setPen(QPen(QColor(0, 120, 212), 2));
                painter->setBrush(QColor(0, 120, 212, 45));
                painter->drawRoundedRect(r, 8, 8);
            } else if (option.state & QStyle::State_MouseOver) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor(255, 255, 255, 22));
                painter->drawRoundedRect(r, 8, 8);
            }

            QFont f = opt.font;
            QString subcat = index.data(EmojiModel::SubcategoryRole).toString();
            QString cat = index.data(EmojiModel::CategoryRole).toString();
            QString codepoints = index.data(EmojiModel::CodepointsRole).toString();
            bool isKaomoji = (subcat == "kaomoji" || cat == "Kaomoji" || codepoints.startsWith("KAO_") ||
                              emoji.contains('(') || emoji.contains(')') || emoji.contains('\\') ||
                              emoji.contains(QChar(0x30c4)) /* ツ */ || emoji.contains(QChar(0x30ce)) /* ノ */);

            if (isKaomoji) {
                f.setPointSize(8);
                f.setFamily("JetBrains Mono, monospace, sans-serif");
            } else {
                f.setPointSize(21);
                f.setFamilies({"Noto Color Emoji", "Segoe UI Emoji", "Apple Color Emoji", "Symbola", "DejaVu Sans"});
            }

            painter->setFont(f);
            painter->setPen(opt.palette.text().color());

            QRect drawRect = opt.rect;
            if (option.state & QStyle::State_MouseOver) {
                drawRect.adjust(-1, -1, 1, 1);
            }

            painter->drawText(drawRect, Qt::AlignCenter, emoji);

            // Subtle skin-tone indicator dot in bottom-right corner
            if (index.data(EmojiModel::HasSkinTonesRole).toBool()) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor(154, 154, 162, 175));
                painter->drawEllipse(r.right() - 5, r.bottom() - 5, 3, 3);
            }

            painter->restore();
            return;
        }
        QStyledItemDelegate::paint(painter, option, index);
    }
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(40, 40);
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

    // Load position from settings
    Settings& settings = Settings::instance();
    QPoint savedPos = settings.panelPos();
    if (savedPos.x() >= 0 && savedPos.y() >= 0) {
        move(savedPos);
    }
    setFixedSize(360, 440);

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

    // Header with search and close button (Stitch UI 36px search field)
    m_headerWidget = new QWidget(m_containerWidget);
    m_headerWidget->setObjectName("headerWidget");
    m_headerWidget->setFixedHeight(50);
    QHBoxLayout* headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(12, 10, 12, 6);
    headerLayout->setSpacing(8);

    // Search icon label
    QLabel* searchIcon = new QLabel(m_headerWidget);
    searchIcon->setText("🔍");
    searchIcon->setFixedSize(18, 18);
    searchIcon->setAlignment(Qt::AlignCenter);

    m_searchEdit = new QLineEdit(m_headerWidget);
    m_searchEdit->setPlaceholderText("Search emoji");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedHeight(36);
    m_searchEdit->setObjectName("searchEdit");
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
    m_closeBtn = new QPushButton("✕", m_headerWidget);
    m_closeBtn->setFixedSize(26, 26);
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
    m_categoryTabBar->setFixedHeight(34);
    m_categoryTabBar->setExpanding(true);
    m_categoryTabBar->setDrawBase(false);
    m_categoryTabBar->setElideMode(Qt::ElideNone);
    m_categoryTabBar->setUsesScrollButtons(false);
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

    // Footer with preview bar (Stitch UI 44px height)
    m_footerWidget = new QWidget(m_containerWidget);
    m_footerWidget->setObjectName("footerWidget");
    m_footerWidget->setFixedHeight(44);
    QHBoxLayout* footerLayout = new QHBoxLayout(m_footerWidget);
    footerLayout->setContentsMargins(12, 4, 12, 4);
    footerLayout->setSpacing(8);

    m_previewEmoji = new QLabel(m_footerWidget);
    m_previewEmoji->setObjectName("previewEmoji");
    m_previewEmoji->setText("✨");
    QFont peFont = m_previewEmoji->font();
    peFont.setPointSize(20);
    m_previewEmoji->setFont(peFont);
    m_previewEmoji->setFixedWidth(30);
    m_previewEmoji->setAlignment(Qt::AlignCenter);

    QVBoxLayout* textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 2, 0, 2);
    textCol->setSpacing(0);

    m_previewName = new QLabel(m_footerWidget);
    m_previewName->setObjectName("previewName");
    m_previewName->setText("Sparkles");
    QFont pnFont = m_previewName->font();
    pnFont.setPointSize(10);
    pnFont.setWeight(QFont::DemiBold);
    m_previewName->setFont(pnFont);

    m_previewShortcode = new QLabel(m_footerWidget);
    m_previewShortcode->setObjectName("previewShortcode");
    m_previewShortcode->setText(":sparkles:");
    QFont psFont = m_previewShortcode->font();
    psFont.setPointSize(9);
    psFont.setFamily("JetBrains Mono, monospace");
    m_previewShortcode->setFont(psFont);

    textCol->addWidget(m_previewName);
    textCol->addWidget(m_previewShortcode);

    m_previewHints = new QLabel(m_footerWidget);
    m_previewHints->setObjectName("previewHints");
    m_previewHints->setText("↵ Insert   ⇧↵ Keep");
    QFont phFont = m_previewHints->font();
    phFont.setPointSize(8);
    phFont.setFamily("JetBrains Mono, monospace");
    m_previewHints->setFont(phFont);
    m_previewHints->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    footerLayout->addWidget(m_previewEmoji);
    footerLayout->addLayout(textCol, 1);
    footerLayout->addWidget(m_previewHints);

    // Assemble
    containerLayout->addWidget(m_headerWidget);
    containerLayout->addWidget(m_categoryTabBar);
    containerLayout->addWidget(m_stackedWidget, 1);
    containerLayout->addWidget(m_footerWidget);

    m_mainLayout->addWidget(m_containerWidget);

    setFixedSize(360, 440);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &EmojiPanel::onSearchTextChanged);
}

void EmojiPanel::populateCategories()
{
    // Remove existing tabs
    while (m_categoryTabBar->count() > 0) {
        m_categoryTabBar->removeTab(0);
    }

    // Standard Unicode Emoji & Kaomoji categories matching Stitch UI specification
    m_categoryDefs.clear();
    m_categoryDefs = {
        {"🕒", "Recently used",        "Recent"},
        {"😀", "Smileys & Emotion",     "Smileys & Emotion"},
        {"👋", "People & Body",         "People & Body"},
        {"🐱", "Animals & Nature",      "Animals & Nature"},
        {"🍕", "Food & Drink",          "Food & Drink"},
        {"⚽", "Activities",            "Activities"},
        {"✈️", "Travel & Places",       "Travel & Places"},
        {"💡", "Objects",               "Objects"},
        {"❤️", "Symbols",              "Symbols"},
        {"🚩", "Flags",                "Flags"},
        {"(•‿•)", "Kaomoji",           "Kaomoji"}
    };

    for (int i = 0; i < static_cast<int>(m_categoryDefs.size()); ++i) {
        m_categoryTabBar->addTab(QString::fromUtf8(m_categoryDefs[i].emoji));
        m_categoryTabBar->setTabToolTip(i, m_categoryDefs[i].tooltip);
    }
}

void EmojiPanel::createCategoryViews()
{
    // Create one list view per tab matching Stitch 8-column layout
    for (int i = 0; i < static_cast<int>(m_categoryDefs.size()); ++i) {
        QListView* listView = new QListView(m_stackedWidget);
        listView->setModel(m_proxyModel);
        listView->setViewMode(QListView::IconMode);
        listView->setIconSize(QSize(36, 36));
        listView->setGridSize(QSize(40, 40));
        listView->setSpacing(2);
        listView->setContentsMargins(6, 4, 6, 4);
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
        f.setPointSize(20);
        f.setFamilies({"Noto Color Emoji","Segoe UI Emoji","Apple Color Emoji","Symbola","DejaVu Sans"});
        listView->setFont(f);
        listView->setStyleSheet("QListView { outline: none; border: none; } QListView::item { padding: 0px; }");
        listView->setItemDelegate(new EmojiDelegate(listView));
        listView->installEventFilter(this);
        connect(listView, &QListView::clicked, this, &EmojiPanel::onEmojiClicked);

        // Right-click custom context menu for skin tone picker
        listView->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(listView, &QListView::customContextMenuRequested, this, [this, listView](const QPoint& pos) {
            QModelIndex idx = listView->indexAt(pos);
            if (idx.isValid()) {
                onSkinToneRequested(idx);
            }
        });

        // Preview on hover / selection
        connect(listView, &QListView::entered, this, [this](const QModelIndex& idx) {
            if (idx.isValid()) {
                QString name = idx.data(EmojiModel::NameRole).toString();
                QString emojiChar = idx.data(EmojiModel::EmojiCharRole).toString();
                updatePreview(emojiChar, name, QString());
            }
        });
        if (listView->selectionModel()) {
            connect(listView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current, const QModelIndex&) {
                if (current.isValid()) {
                    QString name = current.data(EmojiModel::NameRole).toString();
                    QString emojiChar = current.data(EmojiModel::EmojiCharRole).toString();
                    updatePreview(emojiChar, name, QString());
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
                background-color: #2A2A2E;
                border-radius: 12px;
                border: 1px solid rgba(255, 255, 255, 0.08);
            }
            #headerWidget {
                background-color: transparent;
            }
            #searchEdit {
                background-color: rgba(255, 255, 255, 0.07);
                color: #F2F2F4;
                border: 1px solid rgba(255, 255, 255, 0.08);
                border-radius: 8px;
                padding: 0 12px;
                font-size: 13px;
                selection-background-color: #0078D4;
            }
            #searchEdit:focus {
                border: 2px solid #0078D4;
                background-color: rgba(255, 255, 255, 0.1);
            }
            #closeBtn {
                background-color: transparent;
                color: #9A9AA2;
                border: none;
                border-radius: 6px;
                font-size: 12px;
                font-weight: bold;
            }
            #closeBtn:hover {
                background-color: rgba(255, 255, 255, 0.1);
                color: #F2F2F4;
            }
            #categoryTabBar {
                background-color: transparent;
                border: none;
                border-bottom: 1px solid rgba(255, 255, 255, 0.06);
            }
            #categoryTabBar::tab {
                background-color: transparent;
                color: #9A9AA2;
                border: none;
                border-bottom: 2px solid transparent;
                border-radius: 6px;
                padding: 3px 5px;
                margin: 1px 2px;
                font-size: 15px;
                min-width: 24px;
            }
            #categoryTabBar::tab:selected {
                color: #F2F2F4;
                border-bottom: 2px solid #0078D4;
                background-color: rgba(255, 255, 255, 0.06);
            }
            #categoryTabBar::tab:hover:!selected {
                background-color: rgba(255, 255, 255, 0.08);
                color: #F2F2F4;
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
                padding: 4px;
            }
            QScrollBar:vertical {
                background-color: transparent;
                width: 4px;
                border: none;
                margin: 0px;
            }
            QScrollBar::handle:vertical {
                background-color: rgba(120, 120, 130, 0.35);
                border-radius: 2px;
                min-height: 24px;
            }
            QScrollBar::handle:vertical:hover {
                background-color: rgba(120, 120, 130, 0.6);
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
                background: none;
            }
            #footerWidget {
                background-color: rgba(0, 0, 0, 0.2);
                border-top: 1px solid rgba(255, 255, 255, 0.06);
                border-bottom-left-radius: 12px;
                border-bottom-right-radius: 12px;
            }
            #previewEmoji {
                color: #F2F2F4;
            }
            #previewName {
                color: #F2F2F4;
                font-size: 12px;
                font-weight: 600;
            }
            #previewShortcode {
                color: #9A9AA2;
                font-size: 11px;
            }
            #previewHints {
                color: #9A9AA2;
                font-size: 10px;
                background-color: rgba(255, 255, 255, 0.05);
                border: 1px solid rgba(255, 255, 255, 0.08);
                border-radius: 4px;
                padding: 2px 6px;
            }
        )");
    } else {
        setStyleSheet(R"(
            EmojiPanel {
                background-color: transparent;
            }
            #containerWidget {
                background-color: #F6F6F8;
                border-radius: 12px;
                border: 1px solid rgba(0, 0, 0, 0.08);
            }
            #headerWidget {
                background-color: transparent;
            }
            #searchEdit {
                background-color: rgba(0, 0, 0, 0.05);
                color: #1C1C1E;
                border: 1px solid rgba(0, 0, 0, 0.08);
                border-radius: 8px;
                padding: 0 12px;
                font-size: 13px;
                selection-background-color: #0078D4;
            }
            #searchEdit:focus {
                border: 2px solid #0078D4;
                background-color: #FFFFFF;
            }
            #closeBtn {
                background-color: transparent;
                color: #6B6B72;
                border: none;
                border-radius: 6px;
                font-size: 12px;
                font-weight: bold;
            }
            #closeBtn:hover {
                background-color: rgba(0, 0, 0, 0.08);
                color: #1C1C1E;
            }
            #categoryTabBar {
                background-color: transparent;
                border: none;
                border-bottom: 1px solid rgba(0, 0, 0, 0.06);
            }
            #categoryTabBar::tab {
                background-color: transparent;
                color: #6B6B72;
                border: none;
                border-bottom: 2px solid transparent;
                border-radius: 6px;
                padding: 3px 5px;
                margin: 1px 2px;
                font-size: 15px;
                min-width: 24px;
            }
            #categoryTabBar::tab:selected {
                color: #1C1C1E;
                border-bottom: 2px solid #0078D4;
                background-color: rgba(0, 0, 0, 0.05);
            }
            #categoryTabBar::tab:hover:!selected {
                background-color: rgba(0, 0, 0, 0.06);
                color: #1C1C1E;
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
                padding: 4px;
            }
            QScrollBar:vertical {
                background-color: transparent;
                width: 4px;
                border: none;
                margin: 0px;
            }
            QScrollBar::handle:vertical {
                background-color: rgba(0, 0, 0, 0.2);
                border-radius: 2px;
                min-height: 24px;
            }
            QScrollBar::handle:vertical:hover {
                background-color: rgba(0, 0, 0, 0.35);
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
                background: none;
            }
            #footerWidget {
                background-color: rgba(0, 0, 0, 0.03);
                border-top: 1px solid rgba(0, 0, 0, 0.06);
                border-bottom-left-radius: 12px;
                border-bottom-right-radius: 12px;
            }
            #previewEmoji {
                color: #1C1C1E;
            }
            #previewName {
                color: #1C1C1E;
                font-size: 12px;
                font-weight: 600;
            }
            #previewShortcode {
                color: #6B6B72;
                font-size: 11px;
            }
            #previewHints {
                color: #6B6B72;
                font-size: 10px;
                background-color: rgba(0, 0, 0, 0.04);
                border: 1px solid rgba(0, 0, 0, 0.08);
                border-radius: 4px;
                padding: 2px 6px;
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
    m_pendingEmoji.clear();
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
            QModelIndex firstIdx = m_proxyModel->index(0, 0);
            curList->setCurrentIndex(firstIdx);
            if (curList->selectionModel()) {
                curList->selectionModel()->select(firstIdx, QItemSelectionModel::ClearAndSelect);
            }
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

void EmojiPanel::updatePreview(const QString& emoji, const QString& name, const QString& shortcode)
{
    if (m_previewEmoji) {
        m_previewEmoji->setText(emoji.isEmpty() ? "✨" : emoji);
        QFont peFont = m_previewEmoji->font();
        bool isKaomoji = (emoji.contains('(') || emoji.contains(')') || emoji.contains('\\') ||
                          emoji.contains(QChar(0x30c4)) /* ツ */ || emoji.contains(QChar(0x30ce)) /* ノ */);
        if (isKaomoji) {
            peFont.setPointSize(10);
            peFont.setFamily("JetBrains Mono, monospace");
        } else {
            peFont.setPointSize(22);
            peFont.setFamilies({"Noto Color Emoji", "Segoe UI Emoji", "Apple Color Emoji"});
        }
        m_previewEmoji->setFont(peFont);
    }
    if (m_previewName) {
        m_previewName->setText(name.isEmpty() ? "Sparkles" : name);
    }
    if (m_previewShortcode) {
        QString sc = shortcode;
        if (sc.isEmpty() && !name.isEmpty()) {
            QString clean = name.toLower();
            clean.replace(QRegularExpression("[^a-z0-9]+"), "_");
            while (clean.startsWith('_')) clean.remove(0, 1);
            while (clean.endsWith('_')) clean.chop(1);
            sc = QString(":%1:").arg(clean);
        }
        m_previewShortcode->setText(sc);
    }
}

void EmojiPanel::onEmojiClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;

    // Extract emoji and name immediately before anything else
    QString emoji = m_proxyModel->data(index, EmojiModel::EmojiCharRole).toString();
    QString name = m_proxyModel->data(index, EmojiModel::NameRole).toString();
    if (emoji.isEmpty()) return;

    if (m_isInjecting) {
        // Queue this emoji so rapid keystrokes/clicks are not dropped
        m_pendingEmoji = emoji;
        return;
    }

    // Check shift modifier: Shift+Click or Shift+Enter keeps the panel open!
    bool keepOpen = (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier);

    // Update recent/frequent tracking (FIFO up to 20)
    Settings& settings = Settings::instance();
    settings.addRecentEmoji(emoji);
    settings.incrementFrequency(emoji);
    settings.save();

    // Update preview bar
    updatePreview(emoji, name, QString());

    // Direct injection into the target application text box while targetState is valid
    if (m_injector) {
        m_isInjecting = true;
        bool success = m_injector->insertEmoji(emoji, m_targetState);
        if (!success) {
            qWarning() << "Emoji injection failed for:" << emoji;
        } else {
            emit emojiInserted(emoji);
        }
        m_isInjecting = false;

        // Immediately handle any pending injection that was queued during injection
        if (!m_pendingEmoji.isEmpty()) {
            QString nextEmoji = m_pendingEmoji;
            m_pendingEmoji.clear();
            if (m_injector) {
                m_isInjecting = true;
                m_injector->insertEmoji(nextEmoji, m_targetState);
                emit emojiInserted(nextEmoji);
                m_isInjecting = false;
            }
        }
    }

    if (!keepOpen) {
        hidePanel();
    } else if (isVisible()) {
        // If Shift was held, return active focus to EmojiPanel for continuous typing
        if (m_targetState) {
            m_targetState->activateEmojiPanel();
        }
        raise();
        activateWindow();
        int curIdx = m_stackedWidget->currentIndex();
        if (curIdx >= 0 && curIdx < m_listViews.size()) {
            m_listViews[curIdx]->setFocus();
        }

        // If currently viewing the Recent tab, refresh it safely on next event loop tick
        if (curIdx == 0) {
            QTimer::singleShot(50, this, [this]() {
                if (isVisible() && m_stackedWidget->currentIndex() == 0) {
                    Settings& s = Settings::instance();
                    QStringList recentCodes = s.recentEmojis();
                    QVector<EmojiEntry> recent = m_database ? m_database->recentEmojis(recentCodes) : QVector<EmojiEntry>();
                    m_model->setShowRecent(recent);
                }
            });
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
        if (m_proxyModel->rowCount() > 0) {
            QModelIndex firstIdx = m_proxyModel->index(0, 0);
            updatePreview(firstIdx.data(EmojiModel::EmojiCharRole).toString(),
                          firstIdx.data(EmojiModel::NameRole).toString(), QString());
            if (curList) {
                curList->setCurrentIndex(firstIdx);
                if (curList->selectionModel()) {
                    curList->selectionModel()->select(firstIdx, QItemSelectionModel::ClearAndSelect);
                }
            }
        } else {
            updatePreview("🔍", "No emoji found", ":not_found:");
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
        }
        if (m_proxyModel->rowCount() > 0) {
            QModelIndex firstIdx = m_proxyModel->index(0, 0);
            updatePreview(firstIdx.data(EmojiModel::EmojiCharRole).toString(),
                          firstIdx.data(EmojiModel::NameRole).toString(), QString());
            if (curList) {
                curList->setCurrentIndex(firstIdx);
                if (curList->selectionModel()) {
                    curList->selectionModel()->select(firstIdx, QItemSelectionModel::ClearAndSelect);
                }
            }
        } else {
            updatePreview("✨", "Emoji Picker", ":picker:");
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
            QModelIndex firstIdx = m_proxyModel->index(0, 0);
            listView->setCurrentIndex(firstIdx);
            if (listView->selectionModel()) {
                listView->selectionModel()->select(firstIdx, QItemSelectionModel::ClearAndSelect);
            }
        }
        listView->scrollToTop();
    }

    if (m_proxyModel->rowCount() > 0) {
        QModelIndex firstIdx = m_proxyModel->index(0, 0);
        updatePreview(firstIdx.data(EmojiModel::EmojiCharRole).toString(),
                      firstIdx.data(EmojiModel::NameRole).toString(), QString());
    } else if (category == "Recent") {
        updatePreview("🕒", "No recent emojis", ":recent:");
    } else {
        updatePreview(m_categoryDefs[index].emoji, m_categoryDefs[index].tooltip, QString());
    }
}

void EmojiPanel::onSkinToneRequested(const QModelIndex& index)
{
    if (!index.isValid()) return;
    QVariantList variants = index.data(EmojiModel::SkinToneVariantsRole).toList();
    if (variants.isEmpty()) return;

    QMenu* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    menu->setAttribute(Qt::WA_TranslucentBackground);

    QWidget* container = new QWidget(menu);
    container->setObjectName("skinToneContainer");
    container->setStyleSheet(
        "#skinToneContainer {"
        "  background-color: #202024;"
        "  border: 1px solid rgba(255, 255, 255, 0.15);"
        "  border-radius: 8px;"
        "}"
    );
    QHBoxLayout* hlayout = new QHBoxLayout(container);
    hlayout->setContentsMargins(6, 4, 6, 4);
    hlayout->setSpacing(4);

    for (const QVariant& v : variants) {
        QVariantMap map = v.toMap();
        QString toneEmoji = map["emojiChar"].toString();
        QString toneName = map["name"].toString();
        QPushButton* btn = new QPushButton(toneEmoji, container);
        btn->setFixedSize(32, 32);
        btn->setToolTip(toneName);
        QFont tf = btn->font();
        tf.setPointSize(16);
        tf.setFamilies({"Noto Color Emoji", "Segoe UI Emoji", "Apple Color Emoji"});
        btn->setFont(tf);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 6px; }"
            "QPushButton:hover { background: rgba(255, 255, 255, 0.15); }"
        );
        connect(btn, &QPushButton::clicked, menu, [menu, this, toneEmoji, toneName]() {
            menu->close();
            bool keepOpen = (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier);
            Settings& settings = Settings::instance();
            settings.addRecentEmoji(toneEmoji);
            settings.incrementFrequency(toneEmoji);
            settings.save();

            updatePreview(toneEmoji, toneName, QString());

            if (m_injector) {
                m_isInjecting = true;
                bool success = m_injector->insertEmoji(toneEmoji, m_targetState);
                if (success) {
                    emit emojiInserted(toneEmoji);
                }
                m_isInjecting = false;
            }

            if (!keepOpen) {
                hidePanel();
            } else if (isVisible()) {
                if (m_targetState) {
                    m_targetState->activateEmojiPanel();
                }
                raise();
                activateWindow();
            }
        });
        hlayout->addWidget(btn);
    }

    QWidgetAction* action = new QWidgetAction(menu);
    action->setDefaultWidget(container);
    menu->addAction(action);

    int curIdx = m_stackedWidget->currentIndex();
    QListView* curList = (curIdx >= 0 && curIdx < m_listViews.size()) ? m_listViews[curIdx] : nullptr;
    QPoint globalPos;
    if (curList) {
        QRect cellRect = curList->visualRect(index);
        globalPos = curList->mapToGlobal(cellRect.topLeft() - QPoint(10, 44));
    } else {
        globalPos = QCursor::pos();
    }
    menu->popup(globalPos);
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


bool EmojiPanel::navigateGrid(int key)
{
    int curIdx = m_stackedWidget->currentIndex();
    QListView* curList = (curIdx >= 0 && curIdx < m_listViews.size()) ? m_listViews[curIdx] : nullptr;
    if (!curList) return false;

    int totalCount = m_proxyModel->rowCount();
    if (totalCount == 0) {
        if (key == Qt::Key_Up && m_searchEdit) {
            m_searchEdit->setFocus();
        }
        return true;
    }

    // Determine number of columns dynamically, fallback to 8
    int cols = 8;
    if (curList->viewport() && curList->gridSize().width() > 0) {
        int vpWidth = curList->viewport()->width();
        if (vpWidth > 0) {
            cols = qMax(1, vpWidth / curList->gridSize().width());
        }
    }
    if (totalCount >= 2) {
        QRect r0 = curList->visualRect(m_proxyModel->index(0, 0));
        if (r0.isValid() && r0.height() > 0) {
            for (int i = 1; i < qMin(totalCount, 16); ++i) {
                QRect ri = curList->visualRect(m_proxyModel->index(i, 0));
                if (ri.isValid() && ri.y() > r0.y()) {
                    cols = i;
                    break;
                }
            }
        }
    }

    QModelIndex curModelIdx = curList->currentIndex();
    int currentRow = curModelIdx.isValid() ? curModelIdx.row() : -1;
    int nextRow = -1;

    switch (key) {
    case Qt::Key_Right:
        if (currentRow < 0) {
            nextRow = 0;
        } else if (currentRow + 1 < totalCount) {
            nextRow = currentRow + 1;
        } else {
            nextRow = currentRow;
        }
        break;

    case Qt::Key_Left:
        if (currentRow < 0) {
            nextRow = 0;
        } else if (currentRow > 0) {
            nextRow = currentRow - 1;
        } else {
            nextRow = 0;
        }
        break;

    case Qt::Key_Down:
        if (currentRow < 0) {
            nextRow = 0;
        } else if (currentRow + cols < totalCount) {
            nextRow = currentRow + cols;
        } else if (currentRow < totalCount - 1) {
            nextRow = totalCount - 1;
        } else {
            nextRow = currentRow;
        }
        break;

    case Qt::Key_Up:
        if (currentRow < 0 || currentRow < cols) {
            // Reached top row: move focus back into search edit
            if (m_searchEdit) {
                m_searchEdit->setFocus();
                m_searchEdit->setCursorPosition(m_searchEdit->text().length());
            }
            return true;
        } else {
            nextRow = currentRow - cols;
        }
        break;

    case Qt::Key_PageDown: {
        int jump = cols * 5;
        if (currentRow < 0) {
            nextRow = qMin(jump, totalCount - 1);
        } else {
            nextRow = qMin(currentRow + jump, totalCount - 1);
        }
        break;
    }

    case Qt::Key_PageUp: {
        int jump = cols * 5;
        if (currentRow < cols) {
            if (m_searchEdit) {
                m_searchEdit->setFocus();
                m_searchEdit->setCursorPosition(m_searchEdit->text().length());
            }
            return true;
        } else {
            nextRow = qMax(0, currentRow - jump);
        }
        break;
    }

    case Qt::Key_Home:
        nextRow = 0;
        break;

    case Qt::Key_End:
        nextRow = totalCount - 1;
        break;

    default:
        return false;
    }

    if (nextRow >= 0 && nextRow < totalCount) {
        if (!curList->hasFocus()) {
            curList->setFocus();
        }
        QModelIndex newIdx = m_proxyModel->index(nextRow, 0);
        if (newIdx.isValid()) {
            curList->setCurrentIndex(newIdx);
            if (curList->selectionModel()) {
                curList->selectionModel()->select(newIdx, QItemSelectionModel::ClearAndSelect);
            }
            curList->scrollTo(newIdx, QAbstractItemView::EnsureVisible);

            QString name = newIdx.data(EmojiModel::NameRole).toString();
            QString emojiChar = newIdx.data(EmojiModel::EmojiCharRole).toString();
            updatePreview(emojiChar, name, QString());
            return true;
        }
    }

    return true;
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
    case Qt::Key_Up:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
    case Qt::Key_Home:
    case Qt::Key_End:
        event->accept();
        navigateGrid(event->key());
        break;

    default:
        // If user is focused on grid and types printable characters, redirect to search edit
        if (curList && curList->hasFocus() && !event->text().isEmpty() && event->text()[0].isPrint()) {
            if (m_searchEdit) {
                m_searchEdit->setFocus();
                m_searchEdit->setText(m_searchEdit->text() + event->text());
                m_searchEdit->setCursorPosition(m_searchEdit->text().length());
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

        // Tab / Shift+Tab cycling
        if (key == Qt::Key_Tab) {
            if (m_searchEdit && m_searchEdit->hasFocus()) {
                m_categoryTabBar->setFocus();
            } else if (m_categoryTabBar->hasFocus()) {
                if (curList) curList->setFocus();
            } else {
                if (m_searchEdit) m_searchEdit->setFocus();
            }
            return true;
        }
        if (key == Qt::Key_Backtab) {
            if (m_searchEdit && m_searchEdit->hasFocus()) {
                if (curList) curList->setFocus();
            } else if (m_categoryTabBar->hasFocus()) {
                if (m_searchEdit) m_searchEdit->setFocus();
            } else {
                m_categoryTabBar->setFocus();
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

        // When focused on search edit:
        if (watched == m_searchEdit) {
            if (key == Qt::Key_Down) {
                if (curList) {
                    curList->setFocus();
                    if (m_proxyModel->rowCount() > 0) {
                        QModelIndex idx = curList->currentIndex().isValid() ? curList->currentIndex() : m_proxyModel->index(0, 0);
                        curList->setCurrentIndex(idx);
                        if (curList->selectionModel()) {
                            curList->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
                        }
                        curList->scrollTo(idx, QAbstractItemView::EnsureVisible);
                        QString name = idx.data(EmojiModel::NameRole).toString();
                        QString emojiChar = idx.data(EmojiModel::EmojiCharRole).toString();
                        updatePreview(emojiChar, name, QString());
                    }
                }
                return true;
            }
            // Let search edit handle its own Left/Right/Backspace/Text
            return QWidget::eventFilter(watched, event);
        }

        // When focused on category tab bar:
        if (watched == m_categoryTabBar) {
            if (key == Qt::Key_Down) {
                if (curList) {
                    curList->setFocus();
                    if (m_proxyModel->rowCount() > 0) {
                        QModelIndex idx = curList->currentIndex().isValid() ? curList->currentIndex() : m_proxyModel->index(0, 0);
                        curList->setCurrentIndex(idx);
                        if (curList->selectionModel()) {
                            curList->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
                        }
                        curList->scrollTo(idx, QAbstractItemView::EnsureVisible);
                        QString name = idx.data(EmojiModel::NameRole).toString();
                        QString emojiChar = idx.data(EmojiModel::EmojiCharRole).toString();
                        updatePreview(emojiChar, name, QString());
                    }
                }
                return true;
            }
            if (key == Qt::Key_Up) {
                if (m_searchEdit) {
                    m_searchEdit->setFocus();
                    m_searchEdit->setCursorPosition(m_searchEdit->text().length());
                }
                return true;
            }
            // Left/Right handled by QTabBar itself
            return QWidget::eventFilter(watched, event);
        }

        // Grid navigation keys (Up, Down, Left, Right, PageUp, PageDown, Home, End)
        if (key == Qt::Key_Down || key == Qt::Key_Up ||
            key == Qt::Key_Left || key == Qt::Key_Right ||
            key == Qt::Key_PageUp || key == Qt::Key_PageDown ||
            key == Qt::Key_Home || key == Qt::Key_End) {
            if (navigateGrid(key)) {
                return true;
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
                    m_searchEdit->setFocus();
                    m_searchEdit->setText(m_searchEdit->text() + keyEvent->text());
                    m_searchEdit->setCursorPosition(m_searchEdit->text().length());
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