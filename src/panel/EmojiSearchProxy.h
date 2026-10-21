/**
 * EmojiSearchProxy - Filter proxy model for emoji search
 */
#pragma once
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QRegularExpression>

class EmojiSearchProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit EmojiSearchProxy(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {
        setFilterKeyColumn(-1);
        setFilterCaseSensitivity(Qt::CaseInsensitive);
        setDynamicSortFilter(true);
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        if (filterRegularExpression().pattern().isEmpty()) return true;

        QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
        QString name = sourceModel()->data(index, Qt::UserRole + 2).toString(); // NameRole
        QString keywords = sourceModel()->data(index, Qt::UserRole + 3).toString(); // KeywordsRole
        QString emojiChar = sourceModel()->data(index, Qt::UserRole + 6).toString(); // EmojiCharRole

        return name.contains(filterRegularExpression()) || keywords.contains(filterRegularExpression()) || emojiChar.contains(filterRegularExpression());
    }
};
