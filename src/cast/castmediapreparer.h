/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#ifndef CASTMEDIAPREPARER_H
#define CASTMEDIAPREPARER_H

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>

#include <gst/gst.h>

class CastMediaPreparer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(qint64 timelineOffset READ timelineOffset NOTIFY timelineOffsetChanged)

public:
    explicit CastMediaPreparer(const QString &storageDirectory = QString(),
                               QObject *parent = nullptr);
    ~CastMediaPreparer() override;

    bool busy() const;
    QString lastError() const;
    qint64 timelineOffset() const;

    Q_INVOKABLE bool prepareAvi(const QString &inputUrl,
                                const QString &sourceKey,
                                qint64 startPositionMs = 0);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void cancelPreservingOutput();
    Q_INVOKABLE void clearPreparedCache();

signals:
    void busyChanged();
    void lastErrorChanged();
    void timelineOffsetChanged();
    void transcodingStarted(const QString &sourceKey);
    void transcodeProgress(const QString &sourceKey, qint64 bytesWritten);
    void transcodeStreamReady(const QString &sourceKey,
                              const QString &fileUrl,
                              const QString &contentType);
    void ready(const QString &sourceKey,
               const QString &fileUrl,
               const QString &contentType);
    void failed(const QString &sourceKey, const QString &message);

private slots:
    void pollBus();
    void startTranscodeFallback(const QString &reason);

private:
    enum PreparationMode {
        NoPreparationMode = 0,
        RemuxMp4Mode,
        TranscodeWebmMode
    };

    enum SourceSeekPhase {
        SourceSeekNone = 0,
        SourceSeekWarmup,
        SourceSeekPausing,
        SourceSeekSettling,
        SourceSeekPostSeek,
        SourceSeekComplete
    };

    static void demuxPadAddedThunk(GstElement *demux,
                                   GstPad *pad,
                                   gpointer userData);
    static void decodedPadAddedThunk(GstElement *decodebin,
                                     GstPad *pad,
                                     gpointer userData);

    void handleDemuxPadAdded(GstPad *pad);
    void handleDecodedPadAdded(GstElement *decodebin, GstPad *pad);

    bool startRemuxPipeline(QString *errorMessage);
    bool startTranscodePipeline(QString *errorMessage);
    bool configureOutput(PreparationMode mode, QString *errorMessage);

    bool linkPadThroughParser(GstPad *pad, const char *parserFactory);
    bool linkDecodedVideoPad(GstPad *pad);
    bool linkDecodedAudioPad(GstPad *pad);
    bool linkSourceSeekWarmupPad(GstPad *pad, bool video);
    bool activateTranscodeOutputAfterSeek(QString *errorMessage);
    void processTranscodeSourceSeek();
    void releaseSourceSeekWarmupRefs();
    QString diagnosticTag() const;
    void logPipelineState(const char *event) const;
    bool discardPad(GstPad *pad);

    void requestTranscodeFallback(const QString &reason);
    void cancelInternal(bool preserveOutput);
    void teardownPipeline();
    void finishSuccess();
    void finishFailure(const QString &message);
    void setBusy(bool busy);
    void setLastError(const QString &message);

    QString cacheDirectory() const;
    QString normalizedInputUrl(const QString &inputUrl) const;
    QString outputPathForKey(const QString &sourceKey,
                             const QString &suffix) const;
    QString contentTypeForPath(const QString &path) const;
    void clearOldCache();

    QString m_storageDirectory;
    QString m_inputUrl;
    QString m_sourceKey;
    QString m_outputPath;
    QString m_partPath;

    std::atomic<bool> m_busy {false};
    std::atomic<bool> m_videoLinked {false};
    std::atomic<bool> m_audioLinked {false};
    std::atomic<bool> m_fallbackScheduled {false};
    std::atomic<int> m_mode {NoPreparationMode};
    bool m_streamReadyEmitted = false;
    qint64 m_lastProgressBytes = 0;
    quint64 m_diagnosticGeneration = 0;
    qint64 m_lastDiagnosticOutputBytes = -1;
    qint64 m_lastDiagnosticOutputLogMs = 0;
    qint64 m_requestedStartPositionMs = 0;
    qint64 m_timelineOffset = 0;
    SourceSeekPhase m_sourceSeekPhase = SourceSeekNone;
    qint64 m_sourceSeekPhaseStartedMs = 0;
    GstPad *m_sourceSeekVideoPad = nullptr;
    GstPad *m_sourceSeekAudioPad = nullptr;
    GstElement *m_sourceSeekVideoSink = nullptr;
    GstElement *m_sourceSeekAudioSink = nullptr;

    QString m_lastError;
    QHash<QString, QString> m_preparedFiles;

    GstElement *m_pipeline = nullptr;
    GstElement *m_source = nullptr;
    GstElement *m_demux = nullptr;
    GstElement *m_mux = nullptr;
    GstElement *m_sink = nullptr;
    GstBus *m_bus = nullptr;

    QTimer m_busTimer;
};

#endif // CASTMEDIAPREPARER_H
