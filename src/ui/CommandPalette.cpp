#include "CommandPalette.h"
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QStyleOptionViewItem>
#include <algorithm>

// =============================================================================
// Constants
// =============================================================================

static constexpr int kPaletteWidth  = 520;
static constexpr int kMaxHeight     = 420;
static constexpr int kRowHeight     = 34;
static constexpr int kMaxResults    = 12;
static constexpr int kSearchBoxH    = 36;
static constexpr int kBorderRadius  = 8;

// =============================================================================
// Rich-text delegate for highlighted matches
// =============================================================================

class CommandItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        painter->save();

        // Background
        if (option.state & QStyle::State_Selected) {
            painter->fillRect(option.rect, QColor(0x2a, 0x82, 0xda));
        } else if (option.state & QStyle::State_MouseOver) {
            painter->fillRect(option.rect, QColor(0x40, 0x40, 0x40));
        }

        // Columns from UserRole data
        QString richName  = index.data(Qt::UserRole).toString();
        QString shortcut  = index.data(Qt::UserRole + 1).toString();
        QString category  = index.data(Qt::UserRole + 2).toString();

        QRect r = option.rect.adjusted(12, 0, -12, 0);

        // Category tag (right-aligned, small)
        {
            QFont catFont = painter->font();
            catFont.setPixelSize(10);
            painter->setFont(catFont);
            painter->setPen(QColor(0x99, 0x99, 0x99));
            QRect catRect = r;
            catRect.setLeft(catRect.right() - 80);
            painter->drawText(catRect, Qt::AlignVCenter | Qt::AlignRight, category);
        }

        // Shortcut (right of name, dimmer)
        {
            QFont scFont = painter->font();
            scFont.setPixelSize(11);
            painter->setFont(scFont);
            painter->setPen(QColor(0xaa, 0xaa, 0xaa));
            QRect scRect = r;
            scRect.setLeft(scRect.right() - 160);
            scRect.setRight(scRect.right() - 85);
            painter->drawText(scRect, Qt::AlignVCenter | Qt::AlignRight, shortcut);
        }

        // Name with highlighted characters (rich text)
        {
            QTextDocument doc;
            doc.setDefaultFont(option.font);
            QString css = QString(
                "body { color: %1; font-size: 13px; }")
                .arg((option.state & QStyle::State_Selected) ? "#ffffff" : "#dddddd");
            doc.setHtml(QString("<style>%1</style><body>%2</body>").arg(css, richName));
            doc.setDocumentMargin(0);

            painter->translate(r.left(), r.top() + (r.height() - doc.size().height()) / 2.0);
            doc.drawContents(painter);
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& /*option*/,
                   const QModelIndex& /*index*/) const override
    {
        return QSize(kPaletteWidth - 24, kRowHeight);
    }
};

// =============================================================================
// Construction
// =============================================================================

CommandPalette::CommandPalette(QWidget* parent)
    : QWidget(parent)
{
    // Full parent overlay
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    hide();

    // The actual palette panel is laid out by hand inside paintEvent/positioning.
    // We use absolute positioning inside the overlay.

    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("Design Shortcuts — type a command..."));
    m_searchBox->setFixedHeight(kSearchBoxH);
    m_searchBox->setStyleSheet(
        "QLineEdit {"
        "  background: #2a2a2a;"
        "  color: #eee;"
        "  border: none;"
        "  border-bottom: 1px solid #555;"
        "  padding: 0 12px;"
        "  font-size: 14px;"
        "  selection-background-color: #2a82da;"
        "}");
    m_searchBox->installEventFilter(this);

    m_resultList = new QListWidget(this);
    m_resultList->setStyleSheet(
        "QListWidget {"
        "  background: #353535;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QListWidget::item {"
        "  border: none;"
        "  padding: 0;"
        "}"
        "QListWidget::item:selected {"
        "  background: #2a82da;"
        "}"
        "QListWidget::item:hover {"
        "  background: #404040;"
        "}");
    m_resultList->setItemDelegate(new CommandItemDelegate(m_resultList));
    m_resultList->setMouseTracking(true);
    m_resultList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_resultList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultList->installEventFilter(this);

    connect(m_searchBox, &QLineEdit::textChanged, this, &CommandPalette::updateResults);
    connect(m_resultList, &QListWidget::itemActivated, this, [this](QListWidgetItem*) {
        executeSelected();
    });
}

// =============================================================================
// Public API
// =============================================================================

void CommandPalette::setCommands(const std::vector<CommandEntry>& commands)
{
    m_allCommands = commands;
}

void CommandPalette::activate()
{
    if (!parentWidget())
        return;

    // Fill parent
    resize(parentWidget()->size());
    move(0, 0);

    // Position the palette panel centered horizontally, near the top
    int panelX = (width() - kPaletteWidth) / 2;
    int panelY = qMax(60, height() / 6);

    m_searchBox->setGeometry(panelX, panelY, kPaletteWidth, kSearchBoxH);
    m_resultList->setGeometry(panelX, panelY + kSearchBoxH,
                              kPaletteWidth, kMaxResults * kRowHeight);

    m_searchBox->clear();
    updateResults(QString());

    show();
    raise();
    m_searchBox->setFocus();
}

// =============================================================================
// Fuzzy matching
// =============================================================================

bool CommandPalette::fuzzyMatch(const QString& query, const QString& target, int& score) const
{
    if (query.isEmpty()) {
        score = 0;
        return true;
    }

    int qi = 0;
    int gaps = 0;
    int consecutiveBonus = 0;
    bool lastMatched = false;
    score = 0;

    const QString lq = query.toLower();
    const QString lt = target.toLower();

    for (int ti = 0; ti < lt.length() && qi < lq.length(); ++ti) {
        if (lt[ti] == lq[qi]) {
            // Bonus for matching at word start
            if (ti == 0 || target[ti - 1] == ' ')
                score -= 10;
            // Bonus for consecutive matches
            if (lastMatched) {
                consecutiveBonus++;
                score -= consecutiveBonus;
            } else {
                consecutiveBonus = 0;
            }
            lastMatched = true;
            ++qi;
        } else {
            if (qi > 0) gaps++;
            lastMatched = false;
        }
    }

    if (qi < lq.length())
        return false;

    score += gaps * 5;
    // Prefer shorter targets (closer match)
    score += target.length();
    return true;
}

// =============================================================================
// Update results
// =============================================================================

void CommandPalette::updateResults(const QString& query)
{
    m_resultList->clear();
    m_filteredIndices.clear();

    struct ScoredEntry {
        int index;
        int score;
    };
    std::vector<ScoredEntry> scored;
    scored.reserve(m_allCommands.size());

    for (int i = 0; i < static_cast<int>(m_allCommands.size()); ++i) {
        int nameScore = 0, catScore = 0;
        bool nameMatch = fuzzyMatch(query, m_allCommands[i].name, nameScore);
        bool catMatch  = fuzzyMatch(query, m_allCommands[i].category, catScore);
        if (nameMatch || catMatch) {
            int best = nameMatch ? nameScore : (catScore + 50);
            if (nameMatch && catMatch) best = qMin(nameScore, catScore + 50);
            scored.push_back({i, best});
        }
    }

    // Sort by score (ascending = better)
    std::sort(scored.begin(), scored.end(),
              [](const ScoredEntry& a, const ScoredEntry& b) { return a.score < b.score; });

    int shown = 0;
    for (const auto& entry : scored) {
        if (shown >= kMaxResults) break;

        const auto& cmd = m_allCommands[entry.index];

        // Build rich name with matched chars highlighted
        QString richName;
        int qi = 0;
        const QString lq = query.toLower();
        for (int ci = 0; ci < cmd.name.length(); ++ci) {
            bool match = false;
            if (qi < lq.length() && cmd.name[ci].toLower() == lq[qi]) {
                match = true;
                ++qi;
            }
            if (match)
                richName += QString("<b style='color:#5cb8ff;'>%1</b>").arg(cmd.name[ci]);
            else
                richName += cmd.name[ci];
        }

        auto* item = new QListWidgetItem();
        item->setData(Qt::UserRole,     richName);
        item->setData(Qt::UserRole + 1, cmd.shortcut);
        item->setData(Qt::UserRole + 2, cmd.category);
        item->setSizeHint(QSize(kPaletteWidth - 24, kRowHeight));
        m_resultList->addItem(item);
        m_filteredIndices.push_back(entry.index);
        ++shown;
    }

    // Resize result list to content
    int listH = qMin(shown * kRowHeight, kMaxResults * kRowHeight);
    m_resultList->setFixedHeight(listH > 0 ? listH : 0);

    if (m_resultList->count() > 0)
        m_resultList->setCurrentRow(0);
}

// =============================================================================
// Execute
// =============================================================================

void CommandPalette::executeSelected()
{
    int row = m_resultList->currentRow();
    if (row < 0 || row >= static_cast<int>(m_filteredIndices.size()))
        return;

    int cmdIndex = m_filteredIndices[row];
    hide();

    if (m_allCommands[cmdIndex].action)
        m_allCommands[cmdIndex].action();
}

// =============================================================================
// Paint (dim overlay behind the palette)
// =============================================================================

void CommandPalette::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    // Dim background
    p.fillRect(rect(), QColor(0, 0, 0, 140));

    // Palette panel background
    int panelX = m_searchBox->x();
    int panelY = m_searchBox->y();
    int panelH = m_searchBox->height() + m_resultList->height();

    QRectF panelRect(panelX - 1, panelY - 1, kPaletteWidth + 2, panelH + 2);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(0x55, 0x55, 0x55), 1.0));
    p.setBrush(QColor(0x35, 0x35, 0x35));
    p.drawRoundedRect(panelRect, kBorderRadius, kBorderRadius);
}

// =============================================================================
// Key handling
// =============================================================================

void CommandPalette::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool CommandPalette::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);

        if (ke->key() == Qt::Key_Escape) {
            hide();
            return true;
        }

        if (obj == m_searchBox) {
            if (ke->key() == Qt::Key_Down) {
                int next = m_resultList->currentRow() + 1;
                if (next < m_resultList->count())
                    m_resultList->setCurrentRow(next);
                return true;
            }
            if (ke->key() == Qt::Key_Up) {
                int prev = m_resultList->currentRow() - 1;
                if (prev >= 0)
                    m_resultList->setCurrentRow(prev);
                return true;
            }
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                executeSelected();
                return true;
            }
        }

        if (obj == m_resultList) {
            // Forward typing to the search box
            if (ke->key() != Qt::Key_Up && ke->key() != Qt::Key_Down
                && ke->key() != Qt::Key_Return && ke->key() != Qt::Key_Enter) {
                m_searchBox->setFocus();
                QApplication::sendEvent(m_searchBox, event);
                return true;
            }
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                executeSelected();
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

// =============================================================================
// Click outside palette to dismiss
// =============================================================================

void CommandPalette::mousePressEvent(QMouseEvent* event)
{
    // If click is outside the palette panel, dismiss
    QRect panelRect(m_searchBox->x(), m_searchBox->y(),
                    kPaletteWidth, m_searchBox->height() + m_resultList->height());
    if (!panelRect.contains(event->pos())) {
        hide();
    }
}
