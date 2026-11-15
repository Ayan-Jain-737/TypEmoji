/**
 * EmojiSearchProxy - Filter and sorting proxy model for intelligent typo-tolerant emoji search
 */
#pragma once

#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QRegularExpression>
#include <QtCore/QHash>

class EmojiSearchProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit EmojiSearchProxy(QObject* parent = nullptr);
    ~EmojiSearchProxy() override = default;

    void setSearchQuery(const QString& query);
    QString searchQuery() const { return m_searchQuery; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override;

private:
    QString m_searchQuery;
    mutable QHash<int, int> m_scoreCache;

    int getRowScore(int sourceRow, const QModelIndex& sourceParent) const;
};
