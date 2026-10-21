/**
 * EmojiModel implementation
 */
#include "EmojiModel.h"
#include "EmojiDatabase.h"
#include <QtCore/QDebug>
#include <algorithm>

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
    return m_filteredIndices.size() + (m_showSpecialSections ? m_recentEntries.size() + m_frequentEntries.size() : 0);
}

QVariant EmojiModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) return QVariant();

    int row = index.row();
    const EmojiEntry* entry = nullptr;
    bool isSpecial = false;
    QString specialSection;

    // Recent section first
    if (m_showSpecialSections && row < m_recentEntries.size()) {
        entry = &m_recentEntries[row];
        isSpecial = true;
        specialSection = "Recent";
    }
    // Frequent section
    else if (m_showSpecialSections && row < m_recentEntries.size() + m_frequentEntries.size()) {
        int freqIdx = row - m_recentEntries.size();
        entry = &m_frequentEntries[freqIdx];
        isSpecial = true;
        specialSection = "Frequent";
    }
    // Filtered entries
    else {
        int filteredIdx = row - (m_showSpecialSections ? m_recentEntries.size() + m_frequentEntries.size() : 0);
        if (filteredIdx >= 0 && filteredIdx < m_filteredIndices.size()) {
            const auto& all = m_database->allEmojis();
            int idx = m_filteredIndices[filteredIdx];
            if (idx >= 0 && idx < all.size()) {
                entry = &all[idx];
            }
        }
    }

    if (!entry) return QVariant();

    switch (role) {
    case CodepointsRole:
        return entry->codepoints;
    case NameRole:
        return isSpecial ? QString("%1 (%2)").arg(entry->name, specialSection) : entry->name;
    case KeywordsRole:
        return entry->keywords.join(", ");
    case CategoryRole:
        return isSpecial ? specialSection : entry->category;
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
    if (m_currentCategory != category) {
        m_currentCategory = category;
        rebuildFilter();
        emit filterChanged();
    }
}

void EmojiModel::setSearchQuery(const QString& query)
{
    if (m_searchQuery != query) {
        m_searchQuery = query;
        rebuildFilter();
        emit filterChanged();
    }
}

void EmojiModel::setShowRecent(const QVector<EmojiEntry>& recent)
{
    m_recentEntries = recent;
    rebuildFilter();
}

void EmojiModel::setShowFrequent(const QVector<EmojiEntry>& frequent)
{
    m_frequentEntries = frequent;
    rebuildFilter();
}

void EmojiModel::clearFilters()
{
    m_currentCategory.clear();
    m_searchQuery.clear();
    rebuildFilter();
    emit filterChanged();
}

void EmojiModel::showOnlySpecialSections()
{
    m_currentCategory.clear();
    m_searchQuery.clear();
    m_showSpecialSections = true;
    m_filteredIndices.clear();
    beginResetModel();
    endResetModel();
    emit filterChanged();
}

const EmojiEntry* EmojiModel::entryAt(int row) const
{
    if (row < 0 || row >= rowCount()) return nullptr;

    if (m_showSpecialSections && row < m_recentEntries.size()) {
        return &m_recentEntries[row];
    }
    else if (m_showSpecialSections && row < m_recentEntries.size() + m_frequentEntries.size()) {
        return &m_frequentEntries[row - m_recentEntries.size()];
    }
    else {
        int filteredIdx = row - (m_showSpecialSections ? m_recentEntries.size() + m_frequentEntries.size() : 0);
        if (filteredIdx >= 0 && filteredIdx < m_filteredIndices.size()) {
            const auto& all = m_database->allEmojis();
            int idx = m_filteredIndices[filteredIdx];
            if (idx >= 0 && idx < all.size()) {
                return &all[idx];
            }
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
        // Search mode
        QVector<EmojiEntry> results = m_database->search(m_searchQuery);
        m_filteredIndices.reserve(results.size());
        for (const auto& e : results) {
            // Find index in allEmojis by matching codepoints (results are copies, not refs)
            for (int i = 0; i < all.size(); ++i) {
                if (all[i].codepoints == e.codepoints) {
                    m_filteredIndices.append(i);
                    break;
                }
            }
        }
    }
    else if (!m_currentCategory.isEmpty()) {
        // Category filter
        QVector<EmojiEntry> results = m_database->emojisByCategory(m_currentCategory);
        m_filteredIndices.reserve(results.size());
        for (const auto& e : results) {
            for (int i = 0; i < all.size(); ++i) {
                if (all[i].codepoints == e.codepoints) {
                    m_filteredIndices.append(i);
                    break;
                }
            }
        }
    }
    else {
        // All emojis
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