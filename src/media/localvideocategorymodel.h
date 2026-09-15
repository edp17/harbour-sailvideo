/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef LOCALVIDEOCATEGORYMODEL_H
#define LOCALVIDEOCATEGORYMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class LocalVideoModel;

class LocalVideoCategoryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        CountRole,
        SubtitleRole
    };
    Q_ENUM(Roles)

    explicit LocalVideoCategoryModel(LocalVideoModel *sourceModel = nullptr,
                                     QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

signals:
    void countChanged();

private slots:
    void rebuild();

private:
    struct Category {
        QString name;
        QString path;
        QString subtitle;
        int count = 0;
    };

    LocalVideoModel *m_sourceModel = nullptr;
    QVector<Category> m_categories;
};

#endif // LOCALVIDEOCATEGORYMODEL_H
