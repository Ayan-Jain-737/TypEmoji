/**
 * EmojiSearchProxy implementation - Rapid typo-tolerant fuzzy search proxy
 */
#include "EmojiSearchProxy.h"
#include "EmojiDatabase.h"
#include "EmojiModel.h"

EmojiSearchProxy::EmojiSearchProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setFilterKeyColumn(-1);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setDynamicSortFilter(true);
}

void EmojiSearchProxy::setSearchQuery(const QString& query)
{
    QString trimmed = query.trimmed();
    if (m_searchQuery != trimmed) {
        m_searchQuery = trimmed;
        m_scoreCache.clear();
        if (!m_searchQuery.isEmpty()) {
            sort(0, Qt::DescendingOrder);
        } else {
            sort(-1);
        }
        invalidate();
    }
}

int EmojiSearchProxy::getRowScore(int sourceRow, const QModelIndex& sourceParent) const
{
    QString q = m_searchQuery;
    if (q.isEmpty()) {
        q = filterRegularExpression().pattern().trimmed();
    }
    if (q.isEmpty()) return 1;

    auto it = m_scoreCache.constFind(sourceRow);
    if (it != m_scoreCache.constEnd()) {
        return it.value();
    }

    if (!sourceModel()) return 0;
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!index.isValid()) return 0;

    QString name = sourceModel()->data(index, EmojiModel::NameRole).toString();
    QString keywordsStr = sourceModel()->data(index, EmojiModel::KeywordsRole).toString();
    QString emojiChar = sourceModel()->data(index, EmojiModel::EmojiCharRole).toString();
    QString category = sourceModel()->data(index, EmojiModel::CategoryRole).toString();
    bool hasSkinTones = sourceModel()->data(index, EmojiModel::HasSkinTonesRole).toBool();

    if (emojiChar == q || emojiChar.contains(q)) {
        m_scoreCache.insert(sourceRow, 25000);
        return 25000;
    }

    QStringList keywordsList = keywordsStr.split(", ", Qt::SkipEmptyParts);
    int score = EmojiDatabase::calculateMatchScore(q, name, keywordsList, hasSkinTones, category);
    m_scoreCache.insert(sourceRow, score);
    return score;
}

bool EmojiSearchProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QString q = m_searchQuery;
    if (q.isEmpty()) {
        q = filterRegularExpression().pattern().trimmed();
    }
    if (q.isEmpty()) return true;

    return getRowScore(sourceRow, sourceParent) > 0;
}

bool EmojiSearchProxy::lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const
{
    QString q = m_searchQuery;
    if (q.isEmpty()) {
        q = filterRegularExpression().pattern().trimmed();
    }
    if (q.isEmpty()) {
        return source_left.row() < source_right.row();
    }

    int scoreLeft = getRowScore(source_left.row(), source_left.parent());
    int scoreRight = getRowScore(source_right.row(), source_right.parent());

    if (scoreLeft != scoreRight) {
        return scoreLeft < scoreRight;
    }
    return source_left.row() < source_right.row();
}
