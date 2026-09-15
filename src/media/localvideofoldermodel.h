/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef LOCALVIDEOFOLDERMODEL_H
#define LOCALVIDEOFOLDERMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class LocalVideoModel;

class LocalVideoFolderModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QObject *sourceModel READ sourceModel WRITE setSourceModel NOTIFY sourceModelChanged)
    Q_PROPERTY(QString folderPath READ folderPath WRITE setFolderPath NOTIFY folderPathChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        UrlRole,
        SizeTextRole,
        ModifiedRole,
        PathRole
    };
    Q_ENUM(Roles)

    explicit LocalVideoFolderModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QObject *sourceModel() const;
    void setSourceModel(QObject *sourceModel);

    QString folderPath() const;
    void setFolderPath(const QString &folderPath);

    int count() const;

signals:
    void sourceModelChanged();
    void folderPathChanged();
    void countChanged();

private slots:
    void rebuild();

private:
    LocalVideoModel *m_sourceModel = nullptr;
    QString m_folderPath;
    QVector<int> m_rows;
};

#endif // LOCALVIDEOFOLDERMODEL_H
