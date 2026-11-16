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
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtGui/QPainter>
#include <QtGui/QWindow>
#ifdef HAVE_LAYER_SHELL_QT
#include <LayerShellQt/Window>
#endif

class EmojiDelegate : public QStyledItemDelegate {
public:
    explicit EmojiDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_density(Settings::instance().gridDensity())
    {}

    void setDensity(int density) {
        m_density = density;
    }

    int density() const {
        return m_density;
    }

    int cellSize() const {
        switch (m_density) {
        case 0: return 36;
        case 2: return 52;
        case 1:
        default: return 44;
        }
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option);
        Q_UNUSED(index);
        int sz = cellSize();
        return QSize(sz, sz);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        QString emoji = index.data(EmojiModel::EmojiCharRole).toString();
        if (!emoji.isEmpty()) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setRenderHint(QPainter::TextAntialiasing);

            QRect r = opt.rect.adjusted(2, 2, -2, -2);

            // Draw selection background / border
            if (option.state & QStyle::State_Selected) {
                QColor hl = opt.palette.color(QPalette::Highlight);
                painter->setPen(QPen(hl, 1.5));
                hl.setAlpha(55);
                painter->setBrush(hl);
                painter->drawRoundedRect(r, 8, 8);
            } else if (option.state & QStyle::State_MouseOver) {
                QColor hoverColor = opt.palette.color(QPalette::Text);
                hoverColor.setAlpha(26);
                painter->setPen(Qt::NoPen);
                painter->setBrush(hoverColor);
                painter->drawRoundedRect(r, 8, 8);
            }

            QFont f = opt.font;
            QString subcat = index.data(EmojiModel::SubcategoryRole).toString();
            QString cat = index.data(EmojiModel::CategoryRole).toString();
            QString codepoints = index.data(EmojiModel::CodepointsRole).toString();
            bool isKaomoji = (subcat == "kaomoji" || cat == "Kaomoji" || codepoints.startsWith("KAO_") ||
                              emoji.contains('(') || emoji.contains(')') || emoji.contains('\\') ||
                              emoji.contains(QChar(0x30c4)) /* ツ */ || emoji.contains(QChar(0x30ce)) /* ノ */);
            bool isSymbol = (codepoints.startsWith("SYM_") || subcat == "math" || subcat == "currency" ||
                             subcat == "arrow" || subcat == "greek" || subcat == "typography");

            int baseFontSize = 22;
            int kaomojiFontSize = 9;
            int symbolFontSize = 19;
            if (m_density == 0) {
                baseFontSize = 17;
                kaomojiFontSize = 7;
                symbolFontSize = 15;
            } else if (m_density == 2) {
                baseFontSize = 26;
                kaomojiFontSize = 11;
                symbolFontSize = 23;
            }

            if (isKaomoji) {
                f.setPointSize(kaomojiFontSize);
                f.setFamily("JetBrains Mono, monospace, sans-serif");
            } else if (isSymbol) {
                f.setPointSize(symbolFontSize);
                f.setFamilies({"Noto Sans Math", "Noto Sans Symbols", "JetBrains Mono", "sans-serif", "Noto Color Emoji"});
            } else {
                f.setPointSize(baseFontSize);
                f.setFamilies({"Noto Color Emoji", "Apple Color Emoji", "Segoe UI Emoji"});
            }

            painter->setFont(f);
            painter->setPen(opt.palette.text().color());

            // Center emoji cleanly without hover-induced jitter
            painter->drawText(opt.rect, Qt::AlignCenter, emoji);

            // Windows 11-style subtle corner triangle indicator for emojis with skin tone variations
            if (index.data(EmojiModel::HasSkinTonesRole).toBool()) {
                painter->save();
                painter->setRenderHint(QPainter::Antialiasing, true);
                int triSize = (m_density == 0) ? 5 : 6;
                QPolygon tri;
                tri << QPoint(r.right() - 1, r.bottom() - triSize)
                    << QPoint(r.right() - 1, r.bottom() - 1)
                    << QPoint(r.right() - triSize, r.bottom() - 1);
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor(180, 180, 190, 190));
                painter->drawPolygon(tri);
                painter->restore();
            }

            painter->restore();
            return;
        }
        QStyledItemDelegate::paint(painter, option, index);
    }

private:
    int m_density = 1;
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

    setFixedSize(396, 506);

    setFocusPolicy(Qt::StrongFocus);

    m_clickOutsideTimer = new QTimer(this);
    m_clickOutsideTimer->setInterval(80);
    connect(m_clickOutsideTimer, &QTimer::timeout, this, [this]() {
        if (!isVisible() || m_isInjecting || !m_wasActivated) return;

        QWidget* active = QApplication::activeWindow();
        if (active && (active == this || isAncestorOf(active) || active->inherits("QMenu") || active->inherits("QToolTip"))) {
            return;
        }

        if (!isActiveWindow() && !QApplication::activePopupWidget()) {
            hidePanel();
            return;
        }

        if (QGuiApplication::mouseButtons() != Qt::NoButton) {
            if (!rect().contains(mapFromGlobal(QCursor::pos())) && !QApplication::activePopupWidget()) {
                hidePanel();
            }
        }
    });

    m_pressTimer = new QTimer(this);
    m_pressTimer->setSingleShot(true);
    m_pressTimer->setInterval(350);
    connect(m_pressTimer, &QTimer::timeout, this, [this]() {
        if (m_pressedIndex.isValid() && m_pressedIndex.data(EmojiModel::HasSkinTonesRole).toBool()) {
            m_longPressTriggered = true;
            onSkinToneRequested(m_pressedIndex);
        }
    });

    setupUi();
    applyTheme();

    connect(&Settings::instance(), &Settings::defaultSkinToneChanged, this, [this](int tone) {
        Q_UNUSED(tone);
        updateSkinToneBarSelection();
        for (QListView* lv : m_listViews) {
            if (lv && lv->viewport()) {
                lv->viewport()->update();
            }
        }
    });

    if (qApp) {
        qApp->installEventFilter(this);
    }
}

void EmojiPanel::setupUi()
{
    // Main layout with margins for soft floating drop shadow
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(0);

    // Container widget with rounded corners and shadow
    m_containerWidget = new QWidget(this);
    m_containerWidget->setObjectName("containerWidget");

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(m_containerWidget);
    shadow->setBlurRadius(24);
    shadow->setColor(QColor(0, 0, 0, 110));
    shadow->setOffset(0, 6);
    m_containerWidget->setGraphicsEffect(shadow);

    QVBoxLayout* containerLayout = new QVBoxLayout(m_containerWidget);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    // Header with search and close button (52px height)
    m_headerWidget = new QWidget(m_containerWidget);
    m_headerWidget->setObjectName("headerWidget");
    m_headerWidget->setFixedHeight(52);
    QHBoxLayout* headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(12, 10, 12, 6);
    headerLayout->setSpacing(8);

    // Vector magnifying glass icon
    QLabel* searchIcon = new QLabel(m_headerWidget);
    QPixmap searchPix(16, 16);
    searchPix.fill(Qt::transparent);
    {
        QPainter p(&searchPix);
        p.setRenderHint(QPainter::Antialiasing);
        QPen pen(QColor(160, 160, 170), 1.7);
        p.setPen(pen);
        p.drawEllipse(QPointF(6.0, 6.0), 4.2, 4.2);
        p.drawLine(QPointF(9.2, 9.2), QPointF(13.8, 13.8));
    }
    searchIcon->setPixmap(searchPix);
    searchIcon->setFixedSize(16, 16);
    searchIcon->setAlignment(Qt::AlignCenter);

    m_searchEdit = new QLineEdit(m_headerWidget);
    m_searchEdit->setPlaceholderText("Search emojis & kaomoji...");
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

    // Close button (clean circular icon button)
    m_closeBtn = new QPushButton("×", m_headerWidget);
    m_closeBtn->setFixedSize(28, 28);
    m_closeBtn->setObjectName("closeBtn");
    m_closeBtn->setToolTip("Close (Esc)");
    m_closeBtn->setFocusPolicy(Qt::NoFocus);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_closeBtn, &QPushButton::clicked, this, &EmojiPanel::hidePanel);

    headerLayout->addWidget(searchIcon);
    headerLayout->addWidget(m_searchEdit, 1);
    headerLayout->addWidget(m_closeBtn);

    // Category tab bar (Windows-style emoji category tabs)
    m_categoryTabBar = new QTabBar(m_containerWidget);
    m_categoryTabBar->setObjectName("categoryTabBar");
    m_categoryTabBar->setFixedHeight(38);
    m_categoryTabBar->setExpanding(true);
    m_categoryTabBar->setDrawBase(false);
    m_categoryTabBar->setElideMode(Qt::ElideNone);
    m_categoryTabBar->setUsesScrollButtons(false);
    m_categoryTabBar->setMovable(false);
    m_categoryTabBar->setFocusPolicy(Qt::NoFocus);
    m_categoryTabBar->setCursor(Qt::PointingHandCursor);
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

    // Footer with preview bar (52px height)
    m_footerWidget = new QWidget(m_containerWidget);
    m_footerWidget->setObjectName("footerWidget");
    m_footerWidget->setFixedHeight(52);
    QHBoxLayout* footerLayout = new QHBoxLayout(m_footerWidget);
    footerLayout->setContentsMargins(10, 6, 10, 6);
    footerLayout->setSpacing(8);

    m_previewEmoji = new QLabel(m_footerWidget);
    m_previewEmoji->setObjectName("previewEmoji");
    m_previewEmoji->setText("✨");
    QFont peFont = m_previewEmoji->font();
    peFont.setPointSize(22);
    m_previewEmoji->setFont(peFont);
    m_previewEmoji->setFixedSize(38, 38);
    m_previewEmoji->setAlignment(Qt::AlignCenter);

    QVBoxLayout* textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(2);

    m_previewName = new QLabel(m_footerWidget);
    m_previewName->setObjectName("previewName");
    m_previewName->setText("Sparkles");
    QFont pnFont = m_previewName->font();
    pnFont.setPointSize(10);
    pnFont.setWeight(QFont::Bold);
    m_previewName->setFont(pnFont);

    m_previewShortcode = new QLabel(m_footerWidget);
    m_previewShortcode->setObjectName("previewShortcode");
    m_previewShortcode->setText(":sparkles:  •  U+2728");
    QFont psFont = m_previewShortcode->font();
    psFont.setPointSize(8);
    psFont.setFamily("JetBrains Mono, monospace");
    m_previewShortcode->setFont(psFont);

    textCol->addWidget(m_previewName);
    textCol->addWidget(m_previewShortcode);

    m_previewHints = new QLabel(m_footerWidget);
    m_previewHints->setObjectName("previewHints");
    m_previewHints->setTextFormat(Qt::RichText);
    m_previewHints->setText("<span style='font-weight:700;'>↵</span> Insert &nbsp; <span style='font-weight:700;'>⇧↵</span> Keep");
    QFont phFont = m_previewHints->font();
    phFont.setPointSize(8);
    phFont.setFamily("JetBrains Mono, monospace");
    m_previewHints->setFont(phFont);
    m_previewHints->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    footerLayout->addWidget(m_previewEmoji);
    footerLayout->addLayout(textCol, 1);
    footerLayout->addWidget(m_previewHints);

    setupSkinToneBar();

    // Assemble
    containerLayout->addWidget(m_headerWidget);
    containerLayout->addWidget(m_categoryTabBar);
    containerLayout->addWidget(m_skinToneBar);
    containerLayout->addWidget(m_stackedWidget, 1);
    containerLayout->addWidget(m_footerWidget);

    m_mainLayout->addWidget(m_containerWidget);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &EmojiPanel::onSearchTextChanged);
    connect(&Settings::instance(), &Settings::gridDensityChanged, this, &EmojiPanel::updateGridDensity);
}

void EmojiPanel::setupSkinToneBar()
{
    m_skinToneBar = new QWidget(m_containerWidget);
    m_skinToneBar->setObjectName("skinToneBar");
    m_skinToneBar->setFixedHeight(36);

    QHBoxLayout* barLayout = new QHBoxLayout(m_skinToneBar);
    barLayout->setContentsMargins(12, 2, 12, 4);
    barLayout->setSpacing(6);

    QLabel* label = new QLabel("Skin tone:", m_skinToneBar);
    label->setObjectName("skinToneLabel");
    QFont lf = label->font();
    lf.setPointSize(10);
    lf.setWeight(QFont::Medium);
    label->setFont(lf);
    barLayout->addWidget(label);

    barLayout->addStretch(1);

    static const struct ToneSwatch {
        const char* emoji;
        const char* tooltip;
        int toneIndex;
    } s_swatches[6] = {
        {"👋", "Default skin tone (Yellow / Neutral)", 0},
        {"👋🏻", "Light skin tone", 1},
        {"👋🏼", "Medium-light skin tone", 2},
        {"👋🏽", "Medium skin tone", 3},
        {"👋🏾", "Medium-dark skin tone", 4},
        {"👋🏿", "Dark skin tone", 5}
    };

    m_skinToneButtons.clear();
    for (int i = 0; i < 6; ++i) {
        QPushButton* btn = new QPushButton(QString::fromUtf8(s_swatches[i].emoji), m_skinToneBar);
        btn->setFixedSize(28, 28);
        btn->setToolTip(s_swatches[i].tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setProperty("toneIndex", s_swatches[i].toneIndex);
        QFont bf = btn->font();
        bf.setPointSize(14);
        bf.setFamilies({"Noto Color Emoji", "Segoe UI Emoji", "Apple Color Emoji"});
        btn->setFont(bf);

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            Settings::instance().setDefaultSkinTone(i);
            Settings::instance().save();
            updateSkinToneBarSelection();
            for (QListView* lv : m_listViews) {
                if (lv && lv->viewport()) {
                    lv->viewport()->update();
                }
            }
        });

        barLayout->addWidget(btn);
        m_skinToneButtons.append(btn);
    }

    updateSkinToneBarSelection();
    updateSkinToneBarVisibility();
}

void EmojiPanel::updateSkinToneBarSelection()
{
    int currentTone = Settings::instance().defaultSkinTone();
    for (QPushButton* btn : m_skinToneButtons) {
        int tone = btn->property("toneIndex").toInt();
        bool active = (tone == currentTone);
        btn->setProperty("active", active);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
        btn->update();
    }
}

void EmojiPanel::updateSkinToneBarVisibility()
{
    if (!m_skinToneBar) return;

    // When searching
    if (!m_searchEdit->text().trimmed().isEmpty()) {
        bool hasToneResults = false;
        if (m_proxyModel) {
            int rows = std::min(m_proxyModel->rowCount(), 40);
            for (int r = 0; r < rows; ++r) {
                QModelIndex idx = m_proxyModel->index(r, 0);
                if (idx.data(EmojiModel::HasSkinTonesRole).toBool()) {
                    hasToneResults = true;
                    break;
                }
            }
        }
        m_skinToneBar->setVisible(hasToneResults);
        return;
    }

    // By category
    int curTab = m_categoryTabBar->currentIndex();
    if (curTab >= 0 && curTab < static_cast<int>(m_categoryDefs.size())) {
        const QString& cat = m_categoryDefs[curTab].categoryName;
        if (cat == "Objects" || cat == "Symbols" || cat == "Flags" || cat == "Kaomoji") {
            m_skinToneBar->setVisible(false);
            return;
        }
    }
    m_skinToneBar->setVisible(true);
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
    int density = Settings::instance().gridDensity();
    int cellSize = 44;
    int iconSize = 36;
    int fontSize = 22;
    if (density == 0) {
        cellSize = 36;
        iconSize = 28;
        fontSize = 17;
    } else if (density == 2) {
        cellSize = 52;
        iconSize = 44;
        fontSize = 26;
    }

    // Create one list view per tab matching Stitch 8-column layout
    for (int i = 0; i < static_cast<int>(m_categoryDefs.size()); ++i) {
        QListView* listView = new QListView(m_stackedWidget);
        listView->setModel(m_proxyModel);
        listView->setViewMode(QListView::IconMode);
        listView->setIconSize(QSize(iconSize, iconSize));
        listView->setGridSize(QSize(cellSize, cellSize));
        listView->setSpacing(2);
        listView->setContentsMargins(4, 4, 4, 4);
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
        f.setPointSize(fontSize);
        f.setFamilies({"Noto Color Emoji", "Segoe UI Emoji", "Apple Color Emoji"});
        listView->setFont(f);
        listView->setStyleSheet("QListView { outline: none; border: none; } QListView::item { padding: 0px; }");
        auto* delegate = new EmojiDelegate(listView);
        delegate->setDensity(density);
        listView->setItemDelegate(delegate);
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
    QString accent = palette().color(QPalette::Highlight).name();
    if (accent.isEmpty()) {
        accent = "#0078D4";
    }

    bool isDark = (theme == "dark");
    if (theme == "system") {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        isDark = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
        isDark = (palette().color(QPalette::Window).lightness() < 128);
#endif
    }

    if (isDark) {
        QString qss = R"(
            EmojiPanel {
                background-color: transparent;
            }
            #containerWidget {
                background-color: #202124;
                border-radius: 14px;
                border: 1px solid rgba(255, 255, 255, 0.1);
            }
            #headerWidget {
                background-color: transparent;
                border-top-left-radius: 14px;
                border-top-right-radius: 14px;
            }
            #searchEdit {
                background-color: rgba(255, 255, 255, 0.07);
                color: #FFFFFF;
                border: 1px solid rgba(255, 255, 255, 0.1);
                border-radius: 9px;
                padding: 0 10px;
                font-size: 13px;
                selection-background-color: #0078D4;
            }
            #searchEdit:focus {
                border: 1.5px solid #0078D4;
                background-color: rgba(255, 255, 255, 0.1);
            }
            #closeBtn {
                background-color: rgba(255, 255, 255, 0.05);
                color: rgba(255, 255, 255, 0.6);
                border: none;
                border-radius: 14px;
                font-size: 16px;
                font-weight: 500;
            }
            #closeBtn:hover {
                background-color: rgba(255, 255, 255, 0.15);
                color: #FFFFFF;
            }
            #closeBtn:pressed {
                background-color: rgba(255, 255, 255, 0.25);
            }
            #categoryTabBar {
                background-color: transparent;
                border: none;
                border-bottom: 1px solid rgba(255, 255, 255, 0.08);
                padding: 2px 4px;
            }
            #categoryTabBar::tab {
                background-color: transparent;
                color: rgba(255, 255, 255, 0.6);
                border: none;
                border-radius: 8px;
                padding: 4px 2px;
                margin: 2px 2px;
                font-size: 17px;
                min-width: 28px;
            }
            #categoryTabBar::tab:selected {
                color: #FFFFFF;
                background-color: rgba(255, 255, 255, 0.14);
                border-bottom: 2px solid #0078D4;
            }
            #categoryTabBar::tab:hover:!selected {
                background-color: rgba(255, 255, 255, 0.08);
                color: #FFFFFF;
            }
            #skinToneBar {
                background-color: transparent;
                border-bottom: 1px solid rgba(255, 255, 255, 0.08);
            }
            #skinToneLabel {
                color: rgba(255, 255, 255, 0.65);
                font-size: 11px;
                font-weight: 600;
            }
            #skinToneBar QPushButton {
                background-color: rgba(255, 255, 255, 0.05);
                border: 2px solid transparent;
                border-radius: 6px;
            }
            #skinToneBar QPushButton:hover {
                background-color: rgba(255, 255, 255, 0.16);
                border: 2px solid rgba(255, 255, 255, 0.25);
            }
            #skinToneBar QPushButton[active="true"] {
                background-color: rgba(96, 205, 255, 0.22);
                border: 2px solid #0078D4;
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
                width: 6px;
                border: none;
                margin: 4px 2px 4px 0px;
            }
            QScrollBar::handle:vertical {
                background-color: rgba(255, 255, 255, 0.2);
                border-radius: 3px;
                min-height: 28px;
            }
            QScrollBar::handle:vertical:hover {
                background-color: rgba(255, 255, 255, 0.4);
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
                background: none;
            }
            #footerWidget {
                background-color: rgba(0, 0, 0, 0.25);
                border-top: 1px solid rgba(255, 255, 255, 0.08);
                border-bottom-left-radius: 14px;
                border-bottom-right-radius: 14px;
            }
            #previewEmoji {
                background-color: rgba(255, 255, 255, 0.06);
                border: 1px solid rgba(255, 255, 255, 0.08);
                border-radius: 8px;
                color: #FFFFFF;
            }
            #previewName {
                color: #FFFFFF;
                font-size: 12px;
                font-weight: 700;
            }
            #previewShortcode {
                color: rgba(255, 255, 255, 0.65);
                font-size: 11px;
            }
            #previewHints {
                color: rgba(255, 255, 255, 0.7);
                font-size: 10px;
                background-color: rgba(255, 255, 255, 0.06);
                border: 1px solid rgba(255, 255, 255, 0.1);
                border-radius: 5px;
                padding: 2px 6px;
            }
        )";
        qss.replace("#0078D4", accent);
        setStyleSheet(qss);
    } else {
        QString qss = R"(
            EmojiPanel {
                background-color: transparent;
            }
            #containerWidget {
                background-color: #FAFAFC;
                border-radius: 14px;
                border: 1px solid rgba(0, 0, 0, 0.1);
            }
            #headerWidget {
                background-color: transparent;
                border-top-left-radius: 14px;
                border-top-right-radius: 14px;
            }
            #searchEdit {
                background-color: rgba(0, 0, 0, 0.04);
                color: #111111;
                border: 1px solid rgba(0, 0, 0, 0.1);
                border-radius: 9px;
                padding: 0 10px;
                font-size: 13px;
                selection-background-color: #0078D4;
            }
            #searchEdit:focus {
                border: 1.5px solid #0078D4;
                background-color: #FFFFFF;
            }
            #closeBtn {
                background-color: rgba(0, 0, 0, 0.04);
                color: rgba(0, 0, 0, 0.5);
                border: none;
                border-radius: 14px;
                font-size: 16px;
                font-weight: 500;
            }
            #closeBtn:hover {
                background-color: rgba(0, 0, 0, 0.1);
                color: #111111;
            }
            #closeBtn:pressed {
                background-color: rgba(0, 0, 0, 0.18);
            }
            #categoryTabBar {
                background-color: transparent;
                border: none;
                border-bottom: 1px solid rgba(0, 0, 0, 0.08);
                padding: 2px 4px;
            }
            #categoryTabBar::tab {
                background-color: transparent;
                color: rgba(0, 0, 0, 0.6);
                border: none;
                border-radius: 8px;
                padding: 4px 2px;
                margin: 2px 2px;
                font-size: 17px;
                min-width: 28px;
            }
            #categoryTabBar::tab:selected {
                color: #000000;
                background-color: rgba(0, 0, 0, 0.08);
                border-bottom: 2px solid #0078D4;
            }
            #categoryTabBar::tab:hover:!selected {
                background-color: rgba(0, 0, 0, 0.04);
                color: #000000;
            }
            #skinToneBar {
                background-color: transparent;
                border-bottom: 1px solid rgba(0, 0, 0, 0.08);
            }
            #skinToneLabel {
                color: rgba(0, 0, 0, 0.65);
                font-size: 11px;
                font-weight: 600;
            }
            #skinToneBar QPushButton {
                background-color: rgba(0, 0, 0, 0.04);
                border: 2px solid transparent;
                border-radius: 6px;
            }
            #skinToneBar QPushButton:hover {
                background-color: rgba(0, 0, 0, 0.09);
                border: 2px solid rgba(0, 0, 0, 0.18);
            }
            #skinToneBar QPushButton[active="true"] {
                background-color: rgba(0, 120, 212, 0.15);
                border: 2px solid #0078D4;
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
                width: 6px;
                border: none;
                margin: 4px 2px 4px 0px;
            }
            QScrollBar::handle:vertical {
                background-color: rgba(0, 0, 0, 0.2);
                border-radius: 3px;
                min-height: 28px;
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
                border-top: 1px solid rgba(0, 0, 0, 0.08);
                border-bottom-left-radius: 14px;
                border-bottom-right-radius: 14px;
            }
            #previewEmoji {
                background-color: rgba(0, 0, 0, 0.04);
                border: 1px solid rgba(0, 0, 0, 0.08);
                border-radius: 8px;
                color: #111111;
            }
            #previewName {
                color: #111111;
                font-size: 12px;
                font-weight: 700;
            }
            #previewShortcode {
                color: rgba(0, 0, 0, 0.6);
                font-size: 11px;
            }
            #previewHints {
                color: rgba(0, 0, 0, 0.7);
                font-size: 10px;
                background-color: rgba(0, 0, 0, 0.04);
                border: 1px solid rgba(0, 0, 0, 0.08);
                border-radius: 5px;
                padding: 2px 6px;
            }
        )";
        qss.replace("#0078D4", accent);
        setStyleSheet(qss);
    }
}

void EmojiPanel::showAt(const QPoint& pos)
{
    qDebug() << "EmojiPanel::showAt called with pos:" << pos;
    QPoint targetPos = pos;
    if (targetPos.x() < 0 || targetPos.y() < 0) {
        if (m_targetState && m_targetState->isValid() && !m_targetState->caretScreenPos().isNull()) {
            targetPos = m_targetState->caretScreenPos();
        } else {
            targetPos = QCursor::pos();
        }
    }
    positionAtCaret(targetPos);

    // Restore recent from settings
    Settings& settings = Settings::instance();
    QStringList recentCodes = settings.recentEmojis();
    QVector<EmojiEntry> recent = m_database ? m_database->recentEmojis(recentCodes) : QVector<EmojiEntry>();
    m_model->setShowRecent(recent);

    // Default to Smileys & Emotion (index 1) so the picker opens to a rich, vibrant grid of 100+ emojis,
    // or to Recent (index 0) if the user has accumulated a substantial recent history (>= 16 emojis)
    int initialTab = (recent.size() >= 16) ? 0 : 1;
    m_categoryTabBar->setCurrentIndex(initialTab);
    onCategoryChanged(initialTab);
    updateSkinToneBarSelection();
    updateSkinToneBarVisibility();

    m_wasActivated = false;
    m_isInjecting = false;
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
    // Position panel near caret/cursor, centered or slightly offset
    QScreen* screen = QGuiApplication::screenAt(caretPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    QRect screenGeom = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    // If caretPos is at or near (0, 0) or offscreen, center panel on the active screen
    if (caretPos.x() <= 10 && caretPos.y() <= 10) {
        int cx = screenGeom.x() + (screenGeom.width() - width()) / 2;
        int cy = screenGeom.y() + (screenGeom.height() - height()) / 2;
        move(cx, cy);
        return;
    }

    QPoint pos = caretPos + QPoint(10, 15);

    // If panel would overflow bottom, place it above caret
    if (pos.y() + height() > screenGeom.bottom() - 10) {
        pos.setY(caretPos.y() - height() - 10);
    }
    // If panel would overflow right, shift it left
    if (pos.x() + width() > screenGeom.right() - 10) {
        pos.setX(screenGeom.right() - width() - 10);
    }
    // Clamp to screen bounds
    if (pos.x() < screenGeom.left() + 10) {
        pos.setX(screenGeom.left() + 10);
    }
    if (pos.y() < screenGeom.top() + 10) {
        pos.setY(screenGeom.top() + 10);
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

        // Flush any pending recents and panel size to disk immediately
        Settings& settings = Settings::instance();
        settings.setPanelSize(size());
        settings.save();

        if (m_injector) {
            m_injector->flushQueue();
        }

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
        const EmojiEntry* entry = m_database ? m_database->findEmoji(emoji) : nullptr;
        bool isKaomoji = entry ? (entry->category == "Kaomoji" || entry->subcategory == "kaomoji") :
                                 (emoji.contains('(') || emoji.contains(')') || emoji.contains('\\') ||
                                  emoji.contains(QChar(0x30c4)) /* ツ */ || emoji.contains(QChar(0x30ce)) /* ノ */ ||
                                  emoji.contains(QChar(0x0CA0)) /* ಠ */);
        if (isKaomoji) {
            peFont.setPointSize(11);
            peFont.setFamily("JetBrains Mono, monospace");
        } else {
            peFont.setPointSize(22);
            peFont.setFamilies({"Noto Color Emoji", "Segoe UI Emoji", "Apple Color Emoji"});
        }
        m_previewEmoji->setFont(peFont);
    }
    QString rawName = name.isEmpty() ? "Sparkles" : name;
    if (m_previewName) {
        QFontMetrics fm(m_previewName->font());
        m_previewName->setText(fm.elidedText(rawName, Qt::ElideRight, 200));
        m_previewName->setToolTip(rawName);
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
        const EmojiEntry* entry = m_database ? m_database->findEmoji(emoji) : nullptr;
        if (entry && !entry->codepoints.isEmpty()) {
            QStringList cpList = entry->codepoints.split(' ', Qt::SkipEmptyParts);
            QString formattedCp;
            for (const QString& cp : cpList) {
                if (!formattedCp.isEmpty()) formattedCp += " ";
                formattedCp += "U+" + cp;
            }
            if (!sc.isEmpty()) {
                sc += " • " + formattedCp;
            } else {
                sc = formattedCp;
            }
        }
        QFontMetrics fmSc(m_previewShortcode->font());
        m_previewShortcode->setText(fmSc.elidedText(sc, Qt::ElideRight, 200));
        m_previewShortcode->setToolTip(sc);
    }
}

void EmojiPanel::onEmojiClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;

    // Extract emoji and name immediately (EmojiModel::data already accurately resolves the active preferred skin tone variant)
    QString emoji = m_proxyModel->data(index, EmojiModel::EmojiCharRole).toString();
    QString name = m_proxyModel->data(index, EmojiModel::NameRole).toString();
    if (emoji.isEmpty()) return;

    Settings& settings = Settings::instance();

    if (m_isInjecting) {
        // Queue this emoji so rapid keystrokes/clicks are not dropped
        m_pendingEmoji = emoji;
        return;
    }

    // Check shift modifier: Shift+Click or Shift+Enter keeps the panel open!
    bool keepOpen = (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier);

    // Update recent/frequent tracking (FIFO up to 20)
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
    updateSkinToneBarVisibility();
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
    updateSkinToneBarVisibility();
}

void EmojiPanel::onSkinToneRequested(const QModelIndex& index)
{
    if (!index.isValid()) return;
    QVariantList variants = index.data(EmojiModel::SkinToneVariantsRole).toList();
    if (variants.size() <= 1) return;

    Settings& settings = Settings::instance();
    bool isDark = (settings.theme() == "dark");
    if (settings.theme() == "system") {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        isDark = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
        isDark = (palette().color(QPalette::Window).lightness() < 128);
#endif
    }
    QString accent = palette().color(QPalette::Highlight).name();
    if (accent.isEmpty()) {
        accent = isDark ? "#60cdff" : "#0078d4";
    }

    QMenu* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    menu->setAttribute(Qt::WA_TranslucentBackground);

    QWidget* container = new QWidget(menu);
    container->setObjectName("skinToneContainer");
    QString containerStyle = QString(
        "#skinToneContainer {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
    ).arg(isDark ? "#202024" : "#ffffff",
          isDark ? "rgba(255, 255, 255, 0.15)" : "rgba(0, 0, 0, 0.12)");
    container->setStyleSheet(containerStyle);

    QHBoxLayout* hlayout = new QHBoxLayout(container);
    hlayout->setContentsMargins(6, 4, 6, 4);
    hlayout->setSpacing(4);

    int currentTone = settings.defaultSkinTone();

    for (const QVariant& v : variants) {
        QVariantMap map = v.toMap();
        QString toneEmoji = map["emojiChar"].toString();
        QString toneName = map["name"].toString();
        int toneIndex = map["toneIndex"].toInt();

        QPushButton* btn = new QPushButton(toneEmoji, container);
        btn->setFixedSize(34, 34);
        btn->setToolTip(toneName);
        QFont tf = btn->font();
        tf.setPointSize(17);
        tf.setFamilies({"Noto Color Emoji", "Segoe UI Emoji", "Apple Color Emoji"});
        btn->setFont(tf);
        btn->setCursor(Qt::PointingHandCursor);

        bool isActiveTone = (toneIndex == currentTone);
        QString btnStyle;
        if (isActiveTone) {
            btnStyle = QString(
                "QPushButton {"
                "  background: %1;"
                "  border: 2px solid %2;"
                "  border-radius: 6px;"
                "}"
                "QPushButton:hover {"
                "  background: %3;"
                "}"
            ).arg(isDark ? "rgba(96, 205, 255, 0.22)" : "rgba(0, 120, 212, 0.15)",
                  accent,
                  isDark ? "rgba(96, 205, 255, 0.35)" : "rgba(0, 120, 212, 0.25)");
        } else {
            btnStyle = QString(
                "QPushButton {"
                "  background: transparent;"
                "  border: 2px solid transparent;"
                "  border-radius: 6px;"
                "}"
                "QPushButton:hover {"
                "  background: %1;"
                "}"
            ).arg(isDark ? "rgba(255, 255, 255, 0.15)" : "rgba(0, 0, 0, 0.08)");
        }
        btn->setStyleSheet(btnStyle);

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
    if (curList && curList->viewport()) {
        QRect cellRect = curList->visualRect(index);
        QPoint cellGlobal = curList->viewport()->mapToGlobal(cellRect.topLeft());
        int popupH = 46;
        if (cellGlobal.y() - popupH < 10) {
            globalPos = curList->viewport()->mapToGlobal(cellRect.bottomLeft() + QPoint(-10, 4));
        } else {
            globalPos = cellGlobal - QPoint(10, popupH);
        }
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

void EmojiPanel::updateGridDensity(int density)
{
    int cellSize = 44;
    int iconSize = 36;
    int fontSize = 22;

    switch (density) {
    case 0:
        cellSize = 36;
        iconSize = 28;
        fontSize = 17;
        break;
    case 2:
        cellSize = 52;
        iconSize = 44;
        fontSize = 26;
        break;
    case 1:
    default:
        cellSize = 44;
        iconSize = 36;
        fontSize = 22;
        break;
    }

    for (QListView* listView : m_listViews) {
        if (!listView) continue;
        listView->setGridSize(QSize(cellSize, cellSize));
        listView->setIconSize(QSize(iconSize, iconSize));

        QFont f = listView->font();
        f.setPointSize(fontSize);
        f.setFamilies({"Noto Color Emoji", "Segoe UI Emoji", "Apple Color Emoji"});
        listView->setFont(f);

        if (auto* delegate = dynamic_cast<EmojiDelegate*>(listView->itemDelegate())) {
            delegate->setDensity(density);
        }
        listView->doItemsLayout();
    }
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
        return;
    }

    // Handle app shortcuts with Ctrl (Ctrl+Q to quit app, Ctrl+W to close panel)
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Q) {
            event->accept();
            QCoreApplication::quit();
            return;
        }
        if (event->key() == Qt::Key_W) {
            event->accept();
            hidePanel();
            return;
        }
        if (event->key() == Qt::Key_V) {
            event->accept();
            return;
        }
    }

    // Ignore standalone modifier keys
    if (event->key() == Qt::Key_Control ||
        event->key() == Qt::Key_Shift ||
        event->key() == Qt::Key_Alt ||
        event->key() == Qt::Key_Meta ||
        event->key() == Qt::Key_Super_L ||
        event->key() == Qt::Key_Super_R) {
        event->accept();
        return;
    }

    int curIdx = m_stackedWidget->currentIndex();
    QListView* curList = (curIdx >= 0 && curIdx < m_listViews.size()) ? m_listViews[curIdx] : nullptr;

    switch (event->key()) {
    case Qt::Key_Escape:
        event->accept();
        hidePanel();
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
            QWidget* active = QApplication::activeWindow();
            if (active && (active == this || isAncestorOf(active) || active->inherits("QMenu") || QApplication::activePopupWidget())) {
                return;
            }
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

        // Escape: immediately close panel
        if (key == Qt::Key_Escape) {
            hidePanel();
            return true;
        }

        // Ctrl+Q to quit application, Ctrl+W to close panel
        if (keyEvent->modifiers() & Qt::ControlModifier) {
            if (key == Qt::Key_Q) {
                QCoreApplication::quit();
                return true;
            }
            if (key == Qt::Key_W) {
                hidePanel();
                return true;
            }
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

        // Space or Menu key: open skin tone palette if current item has skin tones
        if (key == Qt::Key_Space || key == Qt::Key_Menu) {
            if (watched != m_searchEdit && curList && curList->currentIndex().isValid()) {
                if (curList->currentIndex().data(EmojiModel::HasSkinTonesRole).toBool()) {
                    onSkinToneRequested(curList->currentIndex());
                    return true;
                }
            }
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

    // Handle mouse events: long-press for skin tone picker and click outside to dismiss
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            int curIdx = m_stackedWidget->currentIndex();
            QListView* curList = (curIdx >= 0 && curIdx < m_listViews.size()) ? m_listViews[curIdx] : nullptr;
            if (curList && (watched == curList || watched == curList->viewport())) {
                QPoint pos = (watched == curList->viewport()) ? mouseEvent->pos() : curList->viewport()->mapFrom(curList, mouseEvent->pos());
                m_pressedIndex = curList->indexAt(pos);
                m_longPressTriggered = false;
                if (m_pressedIndex.isValid() && m_pressedIndex.data(EmojiModel::HasSkinTonesRole).toBool()) {
                    QRect cellRect = curList->visualRect(m_pressedIndex);
                    // Single direct click on bottom-right corner triangle indicator opens choice flyout immediately
                    if (pos.x() >= cellRect.right() - 12 && pos.y() >= cellRect.bottom() - 12) {
                        m_longPressTriggered = true;
                        onSkinToneRequested(m_pressedIndex);
                        return true;
                    }
                    m_pressTimer->start();
                }
            }
        }
        // Check if the click is outside our panel geometry
        if (isVisible() && !geometry().contains(mouseEvent->globalPosition().toPoint())) {
            // Don't hide if a popup menu is currently active or if click is on a popup window/menu
            if (QApplication::activePopupWidget()) {
                return false;
            }
            QWidget* clickedWidget = QApplication::widgetAt(mouseEvent->globalPosition().toPoint());
            if (clickedWidget) {
                if (clickedWidget == this || isAncestorOf(clickedWidget)) {
                    return false;
                }
                if (clickedWidget->inherits("QMenu") || (clickedWidget->window() && clickedWidget->window()->inherits("QMenu"))) {
                    return false;
                }
                hidePanel();
            } else {
                hidePanel();
            }
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        if (m_pressTimer && m_pressTimer->isActive()) {
            m_pressTimer->stop();
        }
        if (m_longPressTriggered) {
            m_longPressTriggered = false;
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}