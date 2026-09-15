/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef LOCALVIDEOMODEL_H
#define LOCALVIDEOMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QFutureWatcher>
#include <QSet>
#include <QString>
#include <QVector>

class QFileInfo;

class LocalVideoModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        PathRole,
        UrlRole,
        FolderRole,
        FolderPathRole,
        FolderNameRole,
        SubtitleRole,
        SizeRole,
        SizeTextRole,
        ModifiedRole
    };
    Q_ENUM(Roles)

    struct Entry {
        QString title;
        QString path;
        QString url;
        QString folder;
        QString folderPath;
        QString folderName;
        QString subtitle;
        QString sizeText;
        QString modifiedText;
        qint64 size = 0;
        QDateTime modified;
    };

    explicit LocalVideoModel(QObject *parent = nullptr);
    ~LocalVideoModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    bool scanning() const;
    QString lastError() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString titleAt(int row) const;
    Q_INVOKABLE QString urlAt(int row) const;

    // Read-only access for the category/folder projection models.
    // Kept inline so no separate linker symbol is required.
    const QVector<Entry> &entries() const { return m_entries; }

signals:
    void countChanged();
    void scanningChanged();
    void lastErrorChanged();

private slots:
    void handleScanFinished();

private:
    static QVector<Entry> scanEntries();
    static QVector<Entry> scanTrackerEntries();
    static QVector<Entry> scanFilesystemEntries();
    static QStringList scanRoots();
    static bool isVideoFile(const QString &path);
    static QString prettyFolder(const QString &path);
    static QString prettySize(qint64 bytes);
    static QString subtitleForFile(const QFileInfo &info);
    static Entry entryForFile(const QFileInfo &info);
    static void appendEntryForPath(QVector<Entry> *entries,
                                   QSet<QString> *seen,
                                   const QString &path);

    void setScanning(bool scanning);
    void setLastError(const QString &message);

    QVector<Entry> m_entries;
    QFutureWatcher<QVector<Entry>> m_scanWatcher;
    bool m_scanning = false;
    QString m_lastError;
};

#endif // LOCALVIDEOMODEL_H
