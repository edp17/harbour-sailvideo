/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "localvideocategorymodel.h"
#include "localvideomodel.h"

#include <QHash>

#include <algorithm>

LocalVideoCategoryModel::LocalVideoCategoryModel(LocalVideoModel *sourceModel,
                                                 QObject *parent)
    : QAbstractListModel(parent)
    , m_sourceModel(sourceModel)
{
    if (m_sourceModel) {
        connect(m_sourceModel, &LocalVideoModel::countChanged,
                this, &LocalVideoCategoryModel::rebuild);
    }
    rebuild();
}

int LocalVideoCategoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_categories.size();
}

QVariant LocalVideoCategoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_categories.size()) {
        return QVariant();
    }

    const Category &category = m_categories.at(index.row());
    switch (role) {
    case NameRole:
    case Qt::DisplayRole:
        return category.name;
    case PathRole:
        return category.path;
    case CountRole:
        return category.count;
    case SubtitleRole:
        return category.subtitle;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> LocalVideoCategoryModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(NameRole, "name");
    roles.insert(PathRole, "path");
    roles.insert(CountRole, "videoCount");
    roles.insert(SubtitleRole, "subtitle");
    return roles;
}

int LocalVideoCategoryModel::count() const
{
    return m_categories.size();
}

void LocalVideoCategoryModel::rebuild()
{
    QVector<Category> categories;

    if (m_sourceModel) {
        QHash<QString, int> categoryIndex;
        const QVector<LocalVideoModel::Entry> &entries = m_sourceModel->entries();

        for (const LocalVideoModel::Entry &entry : entries) {
            const QString key = entry.folderPath;
            if (key.trimmed().isEmpty()) {
                continue;
            }

            const auto existing = categoryIndex.constFind(key);
            if (existing == categoryIndex.constEnd()) {
                Category category;
                category.name = entry.folderName.trimmed().isEmpty()
                        ? entry.folder
                        : entry.folderName;
                category.path = entry.folderPath;
                category.subtitle = entry.folder;
                category.count = 1;
                categoryIndex.insert(key, categories.size());
                categories.append(category);
            } else {
                categories[*existing].count += 1;
            }
        }
    }

    std::sort(categories.begin(), categories.end(),
              [](const Category &a, const Category &b) {
        const int nameCompare = QString::localeAwareCompare(a.name, b.name);
        if (nameCompare != 0) {
            return nameCompare < 0;
        }
        return QString::localeAwareCompare(a.path, b.path) < 0;
    });

    beginResetModel();
    m_categories = categories;
    endResetModel();
    emit countChanged();
}
