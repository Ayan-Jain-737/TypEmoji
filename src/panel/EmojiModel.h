/**
 * EmojiModel - Qt model for displaying emojis in the panel
 */
#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QVector>
#include <QtCore/QSortFilterProxyModel>
#include "EmojiDatabase.h"

class EmojiModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        CodepointsRole = Qt::UserRole + 1,
        NameRole,
        KeywordsRole,
        CategoryRole,
        SubcategoryRole,
        EmojiCharRole,
        HasSkinTonesRole,
        IsZwjSequenceRole,
        SkinToneVariantsRole
    };

    explicit EmojiModel(EmojiDatabase* database, QObject* parent = nullptr);
    ~EmojiModel() override = default;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Set filter (category or search query)
    void setCategoryFilter(const QString& category);
    void setSearchQuery(const QString& query);
    void setShowRecent(const QVector<EmojiEntry>& recent);
    void setShowFrequent(const QVector<EmojiEntry>& frequent);
    void clearFilters();
    void showOnlySpecialSections();  // Show only Recent/Frequent sections, no filtered emojis

    // Get entry at index
    const EmojiEntry* entryAt(int row) const;

    // Total count (unfiltered)
    int totalCount() const;

signals:
    void filterChanged();

private:
    EmojiDatabase* m_database = nullptr;
    QVector<int> m_filteredIndices;  // Indices into m_database->allEmojis() - no dangling pointers
    QVector<EmojiEntry> m_recentEntries;
    QVector<EmojiEntry> m_frequentEntries;
    QString m_currentCategory;
    QString m_searchQuery;
    bool m_showSpecialSections = true;
    bool m_isRecentCategory = false;

    void rebuildFilter();
};

class EmojiSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit EmojiSortFilterProxyModel(QObject* parent = nullptr);

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};