/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "localvideofoldermodel.h"
#include "localvideomodel.h"

LocalVideoFolderModel::LocalVideoFolderModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LocalVideoFolderModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant LocalVideoFolderModel::data(const QModelIndex &index, int role) const
{
    if (!m_sourceModel
            || !index.isValid()
            || index.row() < 0
            || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const int sourceRow = m_rows.at(index.row());
    const QVector<LocalVideoModel::Entry> &entries = m_sourceModel->entries();
    if (sourceRow < 0 || sourceRow >= entries.size()) {
        return QVariant();
    }

    const LocalVideoModel::Entry &entry = entries.at(sourceRow);
    switch (role) {
    case TitleRole:
    case Qt::DisplayRole:
        return entry.title;
    case UrlRole:
        return entry.url;
    case SizeTextRole:
        return entry.sizeText;
    case ModifiedRole:
        return entry.modifiedText;
    case PathRole:
        return entry.path;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> LocalVideoFolderModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(TitleRole, "title");
    roles.insert(UrlRole, "url");
    roles.insert(SizeTextRole, "sizeText");
    roles.insert(ModifiedRole, "modified");
    roles.insert(PathRole, "path");
    return roles;
}

QObject *LocalVideoFolderModel::sourceModel() const
{
    return m_sourceModel;
}

void LocalVideoFolderModel::setSourceModel(QObject *sourceModel)
{
    LocalVideoModel *model = qobject_cast<LocalVideoModel *>(sourceModel);
    if (m_sourceModel == model) {
        return;
    }

    if (m_sourceModel) {
        disconnect(m_sourceModel, nullptr, this, nullptr);
    }

    m_sourceModel = model;
    if (m_sourceModel) {
        connect(m_sourceModel, &LocalVideoModel::countChanged,
                this, &LocalVideoFolderModel::rebuild);
    }

    emit sourceModelChanged();
    rebuild();
}

QString LocalVideoFolderModel::folderPath() const
{
    return m_folderPath;
}

void LocalVideoFolderModel::setFolderPath(const QString &folderPath)
{
    if (m_folderPath == folderPath) {
        return;
    }

    m_folderPath = folderPath;
    emit folderPathChanged();
    rebuild();
}

int LocalVideoFolderModel::count() const
{
    return m_rows.size();
}

QString LocalVideoFolderModel::urlAt(int row) const
{
    if (!m_sourceModel || row < 0 || row >= m_rows.size()) {
        return QString();
    }

    const int sourceRow = m_rows.at(row);
    const QVector<LocalVideoModel::Entry> &entries = m_sourceModel->entries();
    return sourceRow >= 0 && sourceRow < entries.size()
            ? entries.at(sourceRow).url
            : QString();
}

QString LocalVideoFolderModel::titleAt(int row) const
{
    if (!m_sourceModel || row < 0 || row >= m_rows.size()) {
        return QString();
    }

    const int sourceRow = m_rows.at(row);
    const QVector<LocalVideoModel::Entry> &entries = m_sourceModel->entries();
    return sourceRow >= 0 && sourceRow < entries.size()
            ? entries.at(sourceRow).title
            : QString();
}

void LocalVideoFolderModel::rebuild()
{
    QVector<int> rows;

    if (m_sourceModel && !m_folderPath.isEmpty()) {
        const QVector<LocalVideoModel::Entry> &entries = m_sourceModel->entries();
        for (int i = 0; i < entries.size(); ++i) {
            if (entries.at(i).folderPath == m_folderPath) {
                rows.append(i);
            }
        }
    }

    beginResetModel();
    m_rows = rows;
    endResetModel();
    emit countChanged();
}
