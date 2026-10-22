/**
 * EmojiModel implementation
 */
#include "EmojiModel.h"
#include "EmojiDatabase.h"
#include <QtCore/QDebug>

EmojiModel::EmojiModel(EmojiDatabase* database, QObject* parent)
    : QAbstractListModel(parent), m_database(database)
{
    if (m_database) {
        connect(m_database, &EmojiDatabase::loaded, this, &EmojiModel::rebuildFilter);
        // If database is already loaded, populate the model immediately
        if (!m_database->allEmojis().isEmpty()) {
            rebuildFilter();
        }
    }
}

int EmojiModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    if (m_isRecentCategory) {
        return m_recentEntries.size();
    }
    return m_filteredIndices.size();
}

QVariant EmojiModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) return QVariant();

    const EmojiEntry* entry = entryAt(index.row());
    if (!entry) return QVariant();

    switch (role) {
    case CodepointsRole:
        return entry->codepoints;
    case NameRole:
        return entry->name;
    case KeywordsRole:
        return entry->keywords.join(", ");
    case CategoryRole:
        return m_isRecentCategory ? QString("Recent") : entry->category;
    case SubcategoryRole:
        return entry->subcategory;
    case EmojiCharRole:
        return entry->emojiChar;
    case HasSkinTonesRole:
        return entry->hasSkinTones;
    case IsZwjSequenceRole:
        return entry->isZwjSequence;
    case SkinToneVariantsRole: {
        if (m_database) {
            QVector<EmojiEntry> variants = m_database->skinToneVariants(*entry);
            QVariantList list;
            for (const auto& v : variants) {
                QVariantMap map;
                map["codepoints"] = v.codepoints;
                map["emojiChar"] = v.emojiChar;
                map["name"] = v.name;
                list.append(map);
            }
            return list;
        }
        return QVariantList();
    }
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> EmojiModel::roleNames() const
{
    return {
        {CodepointsRole, "codepoints"},
        {NameRole, "name"},
        {KeywordsRole, "keywords"},
        {CategoryRole, "category"},
        {SubcategoryRole, "subcategory"},
        {EmojiCharRole, "emojiChar"},
        {HasSkinTonesRole, "hasSkinTones"},
        {IsZwjSequenceRole, "isZwjSequence"},
        {SkinToneVariantsRole, "skinToneVariants"}
    };
}

void EmojiModel::setCategoryFilter(const QString& category)
{
    if (m_currentCategory != category || m_isRecentCategory != (category == "Recent") || !m_searchQuery.isEmpty()) {
        m_currentCategory = category;
        m_isRecentCategory = (category == "Recent");
        m_searchQuery.clear();
        rebuildFilter();
        emit filterChanged();
    }
}

void EmojiModel::setSearchQuery(const QString& query)
{
    if (m_searchQuery != query) {
        m_searchQuery = query;
        if (!query.isEmpty()) {
            m_isRecentCategory = false;
        }
        rebuildFilter();
        emit filterChanged();
    }
}

void EmojiModel::setShowRecent(const QVector<EmojiEntry>& recent)
{
    m_recentEntries = recent;
    if (m_isRecentCategory) {
        rebuildFilter();
    }
}

void EmojiModel::setShowFrequent(const QVector<EmojiEntry>& frequent)
{
    m_frequentEntries = frequent;
}

void EmojiModel::clearFilters()
{
    m_currentCategory.clear();
    m_searchQuery.clear();
    m_isRecentCategory = false;
    rebuildFilter();
    emit filterChanged();
}

void EmojiModel::showOnlySpecialSections()
{
    m_currentCategory = "Recent";
    m_searchQuery.clear();
    m_isRecentCategory = true;
    m_filteredIndices.clear();
    beginResetModel();
    endResetModel();
    emit filterChanged();
}

const EmojiEntry* EmojiModel::entryAt(int row) const
{
    if (row < 0 || row >= rowCount()) return nullptr;

    if (m_isRecentCategory) {
        if (row < m_recentEntries.size()) {
            return &m_recentEntries[row];
        }
        return nullptr;
    }

    if (row >= 0 && row < m_filteredIndices.size() && m_database) {
        const auto& all = m_database->allEmojis();
        int idx = m_filteredIndices[row];
        if (idx >= 0 && idx < all.size()) {
            return &all[idx];
        }
    }
    return nullptr;
}

int EmojiModel::totalCount() const
{
    return m_database ? m_database->count() : 0;
}

void EmojiModel::rebuildFilter()
{
    beginResetModel();

    m_filteredIndices.clear();

    if (!m_database) {
        endResetModel();
        return;
    }

    const auto& all = m_database->allEmojis();

    if (!m_searchQuery.isEmpty()) {
        m_isRecentCategory = false;
        m_filteredIndices = m_database->searchIndices(m_searchQuery);
    }
    else if (m_isRecentCategory || m_currentCategory == "Recent") {
        m_isRecentCategory = true;
        // Rows come directly from m_recentEntries in rowCount/entryAt
    }
    else if (!m_currentCategory.isEmpty() && m_currentCategory != "All") {
        m_isRecentCategory = false;
        QVector<EmojiEntry> results = m_database->emojisByCategory(m_currentCategory);
        m_filteredIndices.reserve(results.size());
        for (const auto& e : results) {
            const EmojiEntry* match = m_database->findByCodepoints(e.codepoints);
            if (match) {
                int idx = static_cast<int>(match - all.constData());
                if (idx >= 0 && idx < all.size()) {
                    m_filteredIndices.append(idx);
                }
            }
        }
    }
    else {
        m_isRecentCategory = false;
        m_filteredIndices.reserve(all.size());
        for (int i = 0; i < all.size(); ++i) {
            m_filteredIndices.append(i);
        }
    }

    endResetModel();
}

EmojiSortFilterProxyModel::EmojiSortFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

bool EmojiSortFilterProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    // Sort by category first, then by name
    QString leftCat = left.data(EmojiModel::CategoryRole).toString();
    QString rightCat = right.data(EmojiModel::CategoryRole).toString();

    if (leftCat != rightCat) {
        return leftCat < rightCat;
    }

    QString leftName = left.data(EmojiModel::NameRole).toString();
    QString rightName = right.data(EmojiModel::NameRole).toString();
    return leftName < rightName;
}