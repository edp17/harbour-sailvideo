/*
 * Copyright (C) 2026 edp17
 *
 * This file is part of SailVideo and is licensed under GPL-3.0-or-later.
 */

#include "castmediapreparer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QStandardPaths>
#include <QUrl>

namespace {

QString defaultStorageDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty()) {
        base = QDir::home().filePath(QStringLiteral(".local/share"));
    }

    const QString organization = QCoreApplication::organizationName().trimmed().isEmpty()
            ? QStringLiteral("org.edp17")
            : QCoreApplication::organizationName().trimmed();
    const QString application = QCoreApplication::applicationName().trimmed().isEmpty()
            ? QStringLiteral("SailVideo")
            : QCoreApplication::applicationName().trimmed();

    return QDir(base).filePath(organization + QLatin1Char('/') + application);
}

QString capsName(GstCaps *caps)
{
    if (!caps || gst_caps_is_empty(caps) || gst_caps_get_size(caps) == 0) {
        return QString();
    }

    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    if (!structure) {
        return QString();
    }

    return QString::fromLatin1(gst_structure_get_name(structure));
}

QString capsDebugString(GstCaps *caps)
{
    if (!caps) {
        return QStringLiteral("<no caps>");
    }

    gchar *text = gst_caps_to_string(caps);
    const QString result = text
            ? QString::fromUtf8(text)
            : QStringLiteral("<unknown caps>");
    g_free(text);
    return result;
}

bool hasFactory(const char *name)
{
    GstElementFactory *factory = gst_element_factory_find(name);
    if (!factory) {
        return false;
    }
    gst_object_unref(factory);
    return true;
}

void syncWithParent(const QList<GstElement *> &elements)
{
    for (GstElement *element : elements) {
        if (element) {
            gst_element_sync_state_with_parent(element);
        }
    }
}

int evenDimension(int value)
{
    if (value < 2) {
        return 2;
    }
    return value - (value % 2);
}

} // namespace

CastMediaPreparer::CastMediaPreparer(const QString &storageDirectory,
                                     QObject *parent)
    : QObject(parent)
    , m_storageDirectory(storageDirectory.trimmed().isEmpty()
                         ? defaultStorageDirectory()
                         : storageDirectory.trimmed())
{
    // QtMultimedia already uses GStreamer on Sailfish. gst_init() is idempotent
    // and lets this Cast-only helper use the same installed framework.
    gst_init(nullptr, nullptr);

    m_busTimer.setInterval(100);
    m_busTimer.setSingleShot(false);
    connect(&m_busTimer, SIGNAL(timeout()), this, SLOT(pollBus()));

    clearOldCache();
}

CastMediaPreparer::~CastMediaPreparer()
{
    cancel();
}

bool CastMediaPreparer::busy() const
{
    return m_busy.load();
}

QString CastMediaPreparer::lastError() const
{
    return m_lastError;
}

qint64 CastMediaPreparer::timelineOffset() const
{
    return m_timelineOffset;
}

QString CastMediaPreparer::cacheDirectory() const
{
    return QDir(m_storageDirectory).filePath(QStringLiteral("cast-remux-cache"));
}

void CastMediaPreparer::clearOldCache()
{
    QDir directory(cacheDirectory());
    if (!directory.exists()) {
        return;
    }

    const QStringList filters = QStringList()
            << QStringLiteral("*.mp4")
            << QStringLiteral("*.webm")
            << QStringLiteral("*.part");
    const QStringList files = directory.entryList(filters, QDir::Files);
    for (const QString &fileName : files) {
        directory.remove(fileName);
    }
}

QString CastMediaPreparer::normalizedInputUrl(const QString &inputUrl) const
{
    const QString clean = inputUrl.trimmed();
    if (clean.isEmpty()) {
        return QString();
    }

    QUrl url(clean);
    if (url.isLocalFile()) {
        const QFileInfo info(url.toLocalFile());
        if (!info.exists() || !info.isFile() || !info.isReadable()) {
            return QString();
        }
        return QUrl::fromLocalFile(info.absoluteFilePath()).toString();
    }

    if (url.scheme().isEmpty()) {
        const QFileInfo info(clean);
        if (info.isAbsolute() && info.exists() && info.isFile() && info.isReadable()) {
            return QUrl::fromLocalFile(info.absoluteFilePath()).toString();
        }
        return QString();
    }

    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https")) {
        return url.toString();
    }

    return QString();
}

QString CastMediaPreparer::outputPathForKey(const QString &sourceKey,
                                             const QString &suffix) const
{
    const QByteArray hash = QCryptographicHash::hash(
                sourceKey.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QDir(cacheDirectory()).filePath(
                QString::fromLatin1(hash)
                + QLatin1Char('.')
                + suffix);
}

QString CastMediaPreparer::contentTypeForPath(const QString &path) const
{
    return path.endsWith(QStringLiteral(".webm"), Qt::CaseInsensitive)
            ? QStringLiteral("video/webm")
            : QStringLiteral("video/mp4");
}

QString CastMediaPreparer::diagnosticTag() const
{
    return QStringLiteral("[AVI prep %1 seek %2ms]")
            .arg(m_diagnosticGeneration)
            .arg(m_requestedStartPositionMs);
}

void CastMediaPreparer::logPipelineState(const char *event) const
{
    if (!m_pipeline) {
        qInfo() << diagnosticTag() << event << "pipeline=null"
                << "phase" << int(m_sourceSeekPhase)
                << "mode" << m_mode.load();
        return;
    }

    GstState current = GST_STATE_VOID_PENDING;
    GstState pending = GST_STATE_VOID_PENDING;
    gst_element_get_state(m_pipeline, &current, &pending, 0);

    qInfo() << diagnosticTag() << event
            << "state" << gst_element_state_get_name(current)
            << "pending" << gst_element_state_get_name(pending)
            << "phase" << int(m_sourceSeekPhase)
            << "videoLinked" << m_videoLinked.load()
            << "audioLinked" << m_audioLinked.load();
}

bool CastMediaPreparer::prepareAvi(const QString &inputUrl,
                                   const QString &sourceKey,
                                   qint64 startPositionMs)
{
    if (m_busy.load()) {
        setLastError(tr("Another AVI file is already being prepared for Chromecast."));
        return false;
    }

    const QString normalized = normalizedInputUrl(inputUrl);
    if (normalized.isEmpty()) {
        setLastError(tr("The AVI playback source cannot be opened for Chromecast preparation."));
        return false;
    }

    const QString key = sourceKey.trimmed().isEmpty()
            ? normalized
            : sourceKey.trimmed();

    m_requestedStartPositionMs = qMax<qint64>(0, startPositionMs);
    ++m_diagnosticGeneration;
    m_lastDiagnosticOutputBytes = -1;
    m_lastDiagnosticOutputLogMs = 0;
    qInfo() << diagnosticTag() << "prepare started";

    if (m_timelineOffset != 0) {
        m_timelineOffset = 0;
        emit timelineOffsetChanged();
    }
    m_sourceSeekPhase = SourceSeekNone;
    m_sourceSeekPhaseStartedMs = 0;
    m_sourceSeekAsyncDone = false;
    releaseSourceSeekWarmupRefs();

    const QString cachedPath = m_preparedFiles.value(key);
    if (!cachedPath.isEmpty()) {
        const QFileInfo cached(cachedPath);
        if (cached.exists() && cached.isFile() && cached.size() > 1024) {
            setLastError(QString());
            const QString fileUrl = QUrl::fromLocalFile(
                        cached.absoluteFilePath()).toString();
            const QString contentType = contentTypeForPath(cached.absoluteFilePath());
            QTimer::singleShot(0, this, [this, key, fileUrl, contentType]() {
                emit ready(key, fileUrl, contentType);
            });
            return true;
        }
        m_preparedFiles.remove(key);
    }

    QDir directory(cacheDirectory());
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        setLastError(tr("Could not create the temporary Chromecast preparation directory."));
        return false;
    }

    m_inputUrl = normalized;
    m_sourceKey = key;
    m_outputPath = outputPathForKey(key, QStringLiteral("mp4"));
    m_partPath = m_outputPath + QStringLiteral(".part");
    QFile::remove(outputPathForKey(key, QStringLiteral("mp4")));
    QFile::remove(outputPathForKey(key, QStringLiteral("webm")));
    QFile::remove(outputPathForKey(key, QStringLiteral("mp4")) + QStringLiteral(".part"));
    QFile::remove(outputPathForKey(key, QStringLiteral("webm")) + QStringLiteral(".part"));

    m_videoLinked.store(false);
    m_audioLinked.store(false);
    m_fallbackScheduled.store(false);
    m_streamReadyEmitted = false;
    m_lastProgressBytes = 0;
    m_mode.store(RemuxMp4Mode);
    setLastError(QString());
    setBusy(true);

    QString error;
    if (!startRemuxPipeline(&error)) {
        teardownPipeline();
        QFile::remove(m_partPath);
        setBusy(false);
        m_mode.store(NoPreparationMode);
        setLastError(error);
        return false;
    }

    qInfo() << diagnosticTag()
            << "SailVideo Cast remux: trying lossless AVI -> MP4 preparation";
    logPipelineState("remux start requested");
    return true;
}

void CastMediaPreparer::cancel()
{
    cancelInternal(false);
}

void CastMediaPreparer::cancelPreservingOutput()
{
    cancelInternal(true);
}

void CastMediaPreparer::cancelInternal(bool preserveOutput)
{
    if (!m_busy.load() && !m_pipeline) {
        return;
    }

    qInfo() << diagnosticTag() << "cancel requested; preserveOutput"
            << preserveOutput << "partPathExists"
            << (!m_partPath.isEmpty() && QFileInfo(m_partPath).exists())
            << "partBytes"
            << (m_partPath.isEmpty() ? 0 : QFileInfo(m_partPath).size());
    logPipelineState("before cancel teardown");

    setBusy(false);
    m_mode.store(NoPreparationMode);
    m_fallbackScheduled.store(false);
    m_streamReadyEmitted = false;
    m_lastProgressBytes = 0;
    teardownPipeline();

    if (!preserveOutput && !m_partPath.isEmpty()) {
        QFile::remove(m_partPath);
    }

    m_inputUrl.clear();
    m_sourceKey.clear();
    m_outputPath.clear();
    m_partPath.clear();
    m_videoLinked.store(false);
    m_audioLinked.store(false);
    m_requestedStartPositionMs = 0;
    m_sourceSeekPhase = SourceSeekNone;
    m_sourceSeekPhaseStartedMs = 0;
    m_sourceSeekAsyncDone = false;
}

void CastMediaPreparer::clearPreparedCache()
{
    if (m_busy.load() || m_pipeline) {
        cancel();
    }

    m_preparedFiles.clear();

    QDir directory(cacheDirectory());
    if (!directory.exists()) {
        return;
    }

    const QStringList filters = QStringList()
            << QStringLiteral("*.mp4")
            << QStringLiteral("*.webm")
            << QStringLiteral("*.part");
    const QStringList files = directory.entryList(filters, QDir::Files);
    for (const QString &fileName : files) {
        directory.remove(fileName);
    }
}

bool CastMediaPreparer::configureOutput(PreparationMode mode,
                                        QString *errorMessage)
{
    if (!m_pipeline) {
        if (errorMessage) {
            *errorMessage = tr("The Chromecast preparation pipeline is unavailable.");
        }
        return false;
    }

    const QString suffix = mode == TranscodeWebmMode
            ? QStringLiteral("webm")
            : QStringLiteral("mp4");
    m_outputPath = outputPathForKey(m_sourceKey, suffix);
    // WebM transcoding is exposed to Chromecast while it is still growing,
    // so keep a stable pathname for the whole session. Lossless MP4 remuxing
    // retains the old .part -> final atomic rename behaviour.
    m_partPath = mode == TranscodeWebmMode
            ? m_outputPath
            : m_outputPath + QStringLiteral(".part");
    QFile::remove(m_outputPath);
    if (m_partPath != m_outputPath) {
        QFile::remove(m_partPath);
    }

    m_mux = gst_element_factory_make(
                mode == TranscodeWebmMode ? "webmmux" : "mp4mux",
                "mux");
    m_sink = gst_element_factory_make("filesink", "sink");
    if (!m_mux || !m_sink) {
        if (errorMessage) {
            *errorMessage = mode == TranscodeWebmMode
                    ? tr("Required GStreamer WebM output components are unavailable.")
                    : tr("Required GStreamer MP4 output components are unavailable.");
        }
        return false;
    }

    if (mode == RemuxMp4Mode) {
        g_object_set(G_OBJECT(m_mux), "faststart", TRUE, nullptr);
    } else {
        // Keep writing clusters progressively, but allow webmmux to finalise
        // duration/cues at EOS. The completed file is then re-exposed through
        // SailVideo's normal Range bridge so Chromecast can seek it.
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(m_mux), "streamable")) {
            g_object_set(G_OBJECT(m_mux), "streamable", FALSE, nullptr);
        }
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(m_mux), "offset-to-zero")) {
            g_object_set(G_OBJECT(m_mux), "offset-to-zero", TRUE, nullptr);
        }
    }

    const QByteArray outputPath = m_partPath.toUtf8();
    g_object_set(G_OBJECT(m_sink), "location", outputPath.constData(), nullptr);

    gst_bin_add_many(GST_BIN(m_pipeline), m_mux, m_sink, nullptr);
    if (!gst_element_link(m_mux, m_sink)) {
        if (errorMessage) {
            *errorMessage = tr("Could not connect the prepared-media muxer to its temporary file.");
        }
        return false;
    }

    return true;
}

bool CastMediaPreparer::startRemuxPipeline(QString *errorMessage)
{
    const QByteArray uri = m_inputUrl.toUtf8();
    GError *uriError = nullptr;

    m_pipeline = gst_pipeline_new("sailvideo-cast-avi-remux");
    m_source = gst_element_make_from_uri(GST_URI_SRC,
                                         uri.constData(),
                                         "source",
                                         &uriError);
    m_demux = gst_element_factory_make("avidemux", "demux");

    if (!m_pipeline || !m_source || !m_demux) {
        QString detail;
        if (uriError && uriError->message) {
            detail = QString::fromUtf8(uriError->message);
        }
        if (uriError) {
            g_error_free(uriError);
        }

        if (errorMessage) {
            *errorMessage = detail.isEmpty()
                    ? tr("Required GStreamer AVI demux components are unavailable.")
                    : tr("Could not open the AVI preparation source: %1").arg(detail);
        }
        return false;
    }

    if (uriError) {
        g_error_free(uriError);
    }

    if (!configureOutput(RemuxMp4Mode, errorMessage)) {
        return false;
    }

    gst_bin_add_many(GST_BIN(m_pipeline),
                     m_source,
                     m_demux,
                     nullptr);

    if (!gst_element_link(m_source, m_demux)) {
        if (errorMessage) {
            *errorMessage = tr("Could not construct the AVI demux pipeline.");
        }
        return false;
    }

    g_signal_connect(m_demux,
                     "pad-added",
                     G_CALLBACK(&CastMediaPreparer::demuxPadAddedThunk),
                     this);

    m_bus = gst_element_get_bus(m_pipeline);

    const GstStateChangeReturn state = gst_element_set_state(
                m_pipeline, GST_STATE_PLAYING);
    if (state == GST_STATE_CHANGE_FAILURE) {
        if (errorMessage) {
            *errorMessage = tr("GStreamer could not start the AVI remux pipeline.");
        }
        return false;
    }

    m_busTimer.start();
    return true;
}

bool CastMediaPreparer::startTranscodePipeline(QString *errorMessage)
{
    if (!hasFactory("uridecodebin")
            || !hasFactory("vp8enc")
            || !hasFactory("vorbisenc")
            || !hasFactory("webmmux")) {
        if (errorMessage) {
            *errorMessage = tr("This AVI requires transcoding, but the required Sailfish GStreamer VP8/Vorbis components are unavailable.");
        }
        return false;
    }

    qInfo() << diagnosticTag() << "creating VP8/Vorbis transcode pipeline";

    m_pipeline = gst_pipeline_new("sailvideo-cast-avi-transcode");
    m_source = gst_element_factory_make("uridecodebin", "decode");

    if (!m_pipeline || !m_source) {
        if (errorMessage) {
            *errorMessage = tr("Could not create the AVI transcoding decoder.");
        }
        return false;
    }

    const bool needsSourceSeek = m_requestedStartPositionMs >= 1000;
    if (!needsSourceSeek) {
        if (!configureOutput(TranscodeWebmMode, errorMessage)) {
            return false;
        }
        m_sourceSeekPhase = SourceSeekComplete;
    } else {
        // Warm the actual AVI decoder first with fakesinks. Device testing of
        // QtMultimedia showed that avidemux seeking is reliable once the
        // streaming pipeline has first been paused and allowed to settle.
        m_sourceSeekPhase = SourceSeekWarmup;
        m_sourceSeekPhaseStartedMs = QDateTime::currentMSecsSinceEpoch();
        qInfo() << diagnosticTag()
                << "SailVideo Cast transcode: warming AVI source before seek to"
                << m_requestedStartPositionMs << "ms";
    }

    const QByteArray uri = m_inputUrl.toUtf8();
    g_object_set(G_OBJECT(m_source), "uri", uri.constData(), nullptr);

    gst_bin_add(GST_BIN(m_pipeline), m_source);

    g_signal_connect(m_source,
                     "pad-added",
                     G_CALLBACK(&CastMediaPreparer::decodedPadAddedThunk),
                     this);

    m_bus = gst_element_get_bus(m_pipeline);

    const GstStateChangeReturn state = gst_element_set_state(
                m_pipeline, GST_STATE_PLAYING);
    qInfo() << diagnosticTag()
            << "transcode PLAYING request return" << int(state);
    logPipelineState("after transcode PLAYING request");
    if (state == GST_STATE_CHANGE_FAILURE) {
        if (errorMessage) {
            *errorMessage = tr("GStreamer could not start the AVI transcoding pipeline.");
        }
        return false;
    }

    m_busTimer.start();
    return true;
}

void CastMediaPreparer::demuxPadAddedThunk(GstElement *demux,
                                           GstPad *pad,
                                           gpointer userData)
{
    Q_UNUSED(demux)
    CastMediaPreparer *self = static_cast<CastMediaPreparer *>(userData);
    if (self) {
        self->handleDemuxPadAdded(pad);
    }
}

void CastMediaPreparer::decodedPadAddedThunk(GstElement *decodebin,
                                             GstPad *pad,
                                             gpointer userData)
{
    CastMediaPreparer *self = static_cast<CastMediaPreparer *>(userData);
    if (self) {
        self->handleDecodedPadAdded(decodebin, pad);
    }
}

void CastMediaPreparer::handleDemuxPadAdded(GstPad *pad)
{
    if (!m_busy.load()
            || m_mode.load() != RemuxMp4Mode
            || m_fallbackScheduled.load()
            || !m_pipeline
            || !pad) {
        return;
    }

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps) {
        caps = gst_pad_query_caps(pad, nullptr);
    }

    const QString name = capsName(caps);
    const QString debugCaps = capsDebugString(caps);

    if (name.startsWith(QStringLiteral("video/"))) {
        if (m_videoLinked.load()) {
            discardPad(pad);
        } else if (name == QLatin1String("video/x-h264")) {
            if (linkPadThroughParser(pad, "h264parse")) {
                m_videoLinked.store(true);
                qInfo() << "SailVideo Cast remux: AVI video is H.264; keeping original video stream";
            } else {
                requestTranscodeFallback(
                            tr("The H.264 stream could not be copied directly."));
            }
        } else {
            qInfo() << "SailVideo Cast remux: AVI video needs transcoding; caps"
                    << debugCaps;
            discardPad(pad);
            requestTranscodeFallback(
                        tr("AVI video is not H.264 and requires transcoding."));
        }
    } else if (name.startsWith(QStringLiteral("audio/"))) {
        if (m_audioLinked.load()) {
            discardPad(pad);
        } else {
            const GstStructure *structure = caps && gst_caps_get_size(caps) > 0
                    ? gst_caps_get_structure(caps, 0)
                    : nullptr;
            gint mpegVersion = 0;
            gint layer = 0;
            const bool hasMpegVersion = structure
                    && gst_structure_get_int(structure, "mpegversion", &mpegVersion);
            const bool hasLayer = structure
                    && gst_structure_get_int(structure, "layer", &layer);

            const char *parser = nullptr;
            if (name == QLatin1String("audio/mpeg")
                    && hasMpegVersion
                    && mpegVersion == 4) {
                parser = "aacparse";
            } else if (name == QLatin1String("audio/mpeg")
                       && hasMpegVersion
                       && mpegVersion == 1
                       && hasLayer
                       && layer == 3) {
                parser = "mpegaudioparse";
            }

            if (parser && linkPadThroughParser(pad, parser)) {
                m_audioLinked.store(true);
                qInfo() << "SailVideo Cast remux: AVI audio is"
                        << (mpegVersion == 4 ? "AAC" : "MP3")
                        << "; keeping original audio stream";
            } else {
                qInfo() << "SailVideo Cast remux: AVI audio needs transcoding; caps"
                        << debugCaps;
                discardPad(pad);
                requestTranscodeFallback(
                            tr("AVI audio cannot be copied directly and requires transcoding."));
            }
        }
    } else {
        discardPad(pad);
    }

    if (caps) {
        gst_caps_unref(caps);
    }
}

void CastMediaPreparer::handleDecodedPadAdded(GstElement *decodebin,
                                              GstPad *pad)
{
    Q_UNUSED(decodebin)

    if (!m_busy.load()
            || m_mode.load() != TranscodeWebmMode
            || !m_pipeline
            || !pad) {
        return;
    }

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps) {
        caps = gst_pad_query_caps(pad, nullptr);
    }

    const QString name = capsName(caps);
    const QString debugCaps = capsDebugString(caps);

    if (m_requestedStartPositionMs >= 1000
            && m_sourceSeekPhase != SourceSeekComplete) {
        if (name.startsWith(QStringLiteral("video/x-raw"))) {
            if (!m_sourceSeekVideoPad
                    && linkSourceSeekWarmupPad(pad, true)) {
                qInfo() << "SailVideo Cast transcode: AVI video warmup pad ready";
            } else if (m_sourceSeekVideoPad) {
                discardPad(pad);
            }
        } else if (name.startsWith(QStringLiteral("audio/x-raw"))) {
            if (!m_sourceSeekAudioPad
                    && linkSourceSeekWarmupPad(pad, false)) {
                qInfo() << "SailVideo Cast transcode: AVI audio warmup pad ready";
            } else if (m_sourceSeekAudioPad) {
                discardPad(pad);
            }
        } else {
            discardPad(pad);
        }

        if (caps) {
            gst_caps_unref(caps);
        }
        return;
    }

    if (name.startsWith(QStringLiteral("video/x-raw"))) {
        if (m_videoLinked.load()) {
            discardPad(pad);
        } else if (linkDecodedVideoPad(pad)) {
            m_videoLinked.store(true);
            qInfo() << "SailVideo Cast transcode: decoded video -> VP8/WebM; caps"
                    << debugCaps;
        } else {
            QMetaObject::invokeMethod(
                        this,
                        "startTranscodeFallback",
                        Qt::QueuedConnection,
                        Q_ARG(QString,
                              tr("The decoded AVI video could not be connected to the VP8 encoder.")));
        }
    } else if (name.startsWith(QStringLiteral("audio/x-raw"))) {
        if (m_audioLinked.load()) {
            discardPad(pad);
        } else if (linkDecodedAudioPad(pad)) {
            m_audioLinked.store(true);
            qInfo() << "SailVideo Cast transcode: decoded audio -> Vorbis/WebM; caps"
                    << debugCaps;
        } else {
            QMetaObject::invokeMethod(
                        this,
                        "startTranscodeFallback",
                        Qt::QueuedConnection,
                        Q_ARG(QString,
                              tr("The decoded AVI audio could not be connected to the Vorbis encoder.")));
        }
    } else {
        discardPad(pad);
    }

    if (caps) {
        gst_caps_unref(caps);
    }
}

bool CastMediaPreparer::linkSourceSeekWarmupPad(GstPad *pad, bool video)
{
    if (!m_pipeline || !pad) {
        return false;
    }

    GstElement *sink = gst_element_factory_make("fakesink", nullptr);
    if (!sink) {
        return false;
    }

    g_object_set(G_OBJECT(sink),
                 "sync", FALSE,
                 "async", FALSE,
                 nullptr);
    gst_bin_add(GST_BIN(m_pipeline), sink);
    gst_element_sync_state_with_parent(sink);

    GstPad *sinkPad = gst_element_get_static_pad(sink, "sink");
    if (!sinkPad) {
        gst_element_set_state(sink, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(m_pipeline), sink);
        return false;
    }

    const GstPadLinkReturn result = gst_pad_link(pad, sinkPad);
    gst_object_unref(sinkPad);
    if (result != GST_PAD_LINK_OK) {
        gst_element_set_state(sink, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(m_pipeline), sink);
        return false;
    }

    gst_object_ref(pad);
    if (video) {
        m_sourceSeekVideoPad = pad;
        m_sourceSeekVideoSink = sink;
        m_sourceSeekPhaseStartedMs = QDateTime::currentMSecsSinceEpoch();
    } else {
        m_sourceSeekAudioPad = pad;
        m_sourceSeekAudioSink = sink;
    }
    return true;
}

bool CastMediaPreparer::activateTranscodeOutputAfterSeek(QString *errorMessage)
{
    if (!m_pipeline || !m_sourceSeekVideoPad) {
        if (errorMessage) {
            *errorMessage = tr("The AVI video stream was not ready after source seeking.");
        }
        return false;
    }

    if (!configureOutput(TranscodeWebmMode, errorMessage)) {
        return false;
    }
    syncWithParent(QList<GstElement *>() << m_mux << m_sink);

    auto replaceWarmupSink = [this](GstPad *pad,
                                    GstElement *sink,
                                    bool video) -> bool {
        if (!pad) {
            return !video;
        }

        if (sink) {
            GstPad *sinkPad = gst_element_get_static_pad(sink, "sink");
            if (sinkPad) {
                gst_pad_unlink(pad, sinkPad);
                gst_object_unref(sinkPad);
            }
            gst_element_set_state(sink, GST_STATE_NULL);
            gst_bin_remove(GST_BIN(m_pipeline), sink);
        }

        return video ? linkDecodedVideoPad(pad)
                     : linkDecodedAudioPad(pad);
    };

    if (!replaceWarmupSink(m_sourceSeekVideoPad,
                           m_sourceSeekVideoSink,
                           true)) {
        if (errorMessage) {
            *errorMessage = tr("The seeked AVI video could not be connected to the VP8 encoder.");
        }
        return false;
    }
    m_videoLinked.store(true);

    if (m_sourceSeekAudioPad) {
        if (!replaceWarmupSink(m_sourceSeekAudioPad,
                               m_sourceSeekAudioSink,
                               false)) {
            if (errorMessage) {
                *errorMessage = tr("The seeked AVI audio could not be connected to the Vorbis encoder.");
            }
            return false;
        }
        m_audioLinked.store(true);
    }

    m_sourceSeekVideoSink = nullptr;
    m_sourceSeekAudioSink = nullptr;
    if (m_sourceSeekVideoPad) {
        gst_object_unref(m_sourceSeekVideoPad);
        m_sourceSeekVideoPad = nullptr;
    }
    if (m_sourceSeekAudioPad) {
        gst_object_unref(m_sourceSeekAudioPad);
        m_sourceSeekAudioPad = nullptr;
    }

    if (m_timelineOffset != m_requestedStartPositionMs) {
        m_timelineOffset = m_requestedStartPositionMs;
        emit timelineOffsetChanged();
    }

    m_sourceSeekPhase = SourceSeekComplete;
    m_sourceSeekPhaseStartedMs = QDateTime::currentMSecsSinceEpoch();

    const GstStateChangeReturn state =
            gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (state == GST_STATE_CHANGE_FAILURE) {
        if (errorMessage) {
            *errorMessage = tr("GStreamer could not resume AVI transcoding after seeking.");
        }
        return false;
    }

    qInfo() << diagnosticTag()
            << "SailVideo Cast transcode: source seek ready; WebM timeline 0 maps to AVI"
            << m_timelineOffset << "ms";
    logPipelineState("after source-seek PLAYING request");
    return true;
}

void CastMediaPreparer::processTranscodeSourceSeek()
{
    if (!m_busy.load()
            || m_mode.load() != TranscodeWebmMode
            || !m_pipeline
            || m_requestedStartPositionMs < 1000
            || m_sourceSeekPhase == SourceSeekComplete
            || m_sourceSeekPhase == SourceSeekNone) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (m_sourceSeekPhase == SourceSeekWarmup) {
        if (!m_sourceSeekVideoPad) {
            return;
        }

        if (now - m_sourceSeekPhaseStartedMs < 650) {
            return;
        }

        qInfo() << diagnosticTag()
                << "SailVideo Cast transcode: pausing warmed AVI pipeline before source seek";
        const GstStateChangeReturn state =
                gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
        qInfo() << diagnosticTag()
                << "source-seek PAUSED request return" << int(state);
        if (state == GST_STATE_CHANGE_FAILURE) {
            finishFailure(tr("The AVI transcode pipeline could not pause before seeking."));
            return;
        }
        m_sourceSeekPhase = SourceSeekPausing;
        m_sourceSeekPhaseStartedMs = now;
        return;
    }

    if (m_sourceSeekPhase == SourceSeekPausing) {
        GstState state = GST_STATE_VOID_PENDING;
        GstState pending = GST_STATE_VOID_PENDING;
        gst_element_get_state(m_pipeline, &state, &pending, 0);

        if (state == GST_STATE_PAUSED) {
            qInfo() << diagnosticTag()
                    << "SailVideo Cast transcode: PausedState reached before source seek";
            m_sourceSeekPhase = SourceSeekSettling;
            m_sourceSeekPhaseStartedMs = now;
            return;
        }

        if (now - m_sourceSeekPhaseStartedMs > 5000) {
            finishFailure(tr("The AVI transcode pipeline did not pause in time for seeking."));
        }
        return;
    }

    if (m_sourceSeekPhase == SourceSeekSettling) {
        if (now - m_sourceSeekPhaseStartedMs < 500) {
            return;
        }

        qInfo() << diagnosticTag()
                << "SailVideo Cast transcode: issuing paused source seek to"
                << m_requestedStartPositionMs << "ms";

        // A flushing GStreamer seek completes asynchronously. Drop any
        // ASYNC_DONE left over from the initial preroll so the next one can be
        // unambiguously attributed to this source seek.
        if (m_bus) {
            for (;;) {
                GstMessage *staleAsync = gst_bus_pop_filtered(
                            m_bus, GST_MESSAGE_ASYNC_DONE);
                if (!staleAsync) {
                    break;
                }
                qInfo() << diagnosticTag()
                        << "discarding pre-seek ASYNC_DONE";
                gst_message_unref(staleAsync);
            }
        }
        m_sourceSeekAsyncDone = false;

        const gboolean seeked = gst_element_seek_simple(
                    m_pipeline,
                    GST_FORMAT_TIME,
                    static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH
                                              | GST_SEEK_FLAG_KEY_UNIT),
                    m_requestedStartPositionMs * GST_MSECOND);
        qInfo() << diagnosticTag()
                << "source seek result" << bool(seeked);
        if (!seeked) {
            finishFailure(tr("The AVI source could not seek to the requested Chromecast position."));
            return;
        }

        m_sourceSeekPhase = SourceSeekPostSeek;
        m_sourceSeekPhaseStartedMs = now;
        return;
    }

    if (m_sourceSeekPhase == SourceSeekPostSeek) {
        // gst_element_seek_simple() returning TRUE only means the seek was
        // accepted. Do not tear down the warm-up sinks and attach the WebM
        // encoders until the flushing seek has actually completed preroll.
        if (!m_sourceSeekAsyncDone) {
            if (now - m_sourceSeekPhaseStartedMs > 5000) {
                finishFailure(tr("The AVI source seek did not finish preroll in time."));
            }
            return;
        }

        qInfo() << diagnosticTag()
                << "post-seek ASYNC_DONE confirmed; activating WebM output";

        gint64 position = GST_CLOCK_TIME_NONE;
        if (gst_element_query_position(m_pipeline,
                                       GST_FORMAT_TIME,
                                       &position)
                && position != GST_CLOCK_TIME_NONE) {
            qInfo() << diagnosticTag()
                    << "SailVideo Cast transcode: paused source reports"
                    << (position / GST_MSECOND) << "ms after seek";
        }

        QString error;
        if (!activateTranscodeOutputAfterSeek(&error)) {
            finishFailure(error);
        }
    }
}

void CastMediaPreparer::releaseSourceSeekWarmupRefs()
{
    if (m_sourceSeekVideoPad) {
        gst_object_unref(m_sourceSeekVideoPad);
        m_sourceSeekVideoPad = nullptr;
    }
    if (m_sourceSeekAudioPad) {
        gst_object_unref(m_sourceSeekAudioPad);
        m_sourceSeekAudioPad = nullptr;
    }
    m_sourceSeekVideoSink = nullptr;
    m_sourceSeekAudioSink = nullptr;
}

bool CastMediaPreparer::linkPadThroughParser(GstPad *pad,
                                             const char *parserFactory)
{
    GstElement *queue = gst_element_factory_make("queue", nullptr);
    GstElement *parser = gst_element_factory_make(parserFactory, nullptr);
    if (!queue || !parser) {
        if (queue) gst_object_unref(queue);
        if (parser) gst_object_unref(parser);
        return false;
    }

    gst_bin_add_many(GST_BIN(m_pipeline), queue, parser, nullptr);

    if (!gst_element_link(queue, parser)
            || !gst_element_link(parser, m_mux)) {
        gst_element_set_state(queue, GST_STATE_NULL);
        gst_element_set_state(parser, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(m_pipeline), queue);
        gst_bin_remove(GST_BIN(m_pipeline), parser);
        return false;
    }

    syncWithParent(QList<GstElement *>() << queue << parser);

    GstPad *queueSink = gst_element_get_static_pad(queue, "sink");
    if (!queueSink) {
        return false;
    }

    const GstPadLinkReturn result = gst_pad_link(pad, queueSink);
    gst_object_unref(queueSink);
    return result == GST_PAD_LINK_OK;
}

bool CastMediaPreparer::linkDecodedVideoPad(GstPad *pad)
{
    GstElement *queue = gst_element_factory_make("queue", nullptr);
    GstElement *convert = gst_element_factory_make("videoconvert", nullptr);
    GstElement *scale = gst_element_factory_make("videoscale", nullptr);
    GstElement *filter = gst_element_factory_make("capsfilter", nullptr);
    GstElement *encoder = gst_element_factory_make("vp8enc", nullptr);
    GstElement *outputQueue = gst_element_factory_make("queue", nullptr);

    if (!queue || !convert || !scale || !filter || !encoder || !outputQueue) {
        if (queue) gst_object_unref(queue);
        if (convert) gst_object_unref(convert);
        if (scale) gst_object_unref(scale);
        if (filter) gst_object_unref(filter);
        if (encoder) gst_object_unref(encoder);
        if (outputQueue) gst_object_unref(outputQueue);
        return false;
    }

    int sourceWidth = 0;
    int sourceHeight = 0;
    GstCaps *inputCaps = gst_pad_get_current_caps(pad);
    if (inputCaps && gst_caps_get_size(inputCaps) > 0) {
        const GstStructure *structure = gst_caps_get_structure(inputCaps, 0);
        if (structure) {
            gst_structure_get_int(structure, "width", &sourceWidth);
            gst_structure_get_int(structure, "height", &sourceHeight);
        }
    }

    int targetWidth = sourceWidth;
    int targetHeight = sourceHeight;
    if (sourceWidth > 0 && sourceHeight > 0
            && (sourceWidth > 1280 || sourceHeight > 720)) {
        const double widthScale = 1280.0 / double(sourceWidth);
        const double heightScale = 720.0 / double(sourceHeight);
        const double factor = qMin(widthScale, heightScale);
        targetWidth = evenDimension(int(sourceWidth * factor));
        targetHeight = evenDimension(int(sourceHeight * factor));
    }

    GstCaps *rawCaps = nullptr;
    if (targetWidth > 0 && targetHeight > 0) {
        rawCaps = gst_caps_new_simple("video/x-raw",
                                     "format", G_TYPE_STRING, "I420",
                                     "width", G_TYPE_INT, targetWidth,
                                     "height", G_TYPE_INT, targetHeight,
                                     nullptr);
    } else {
        rawCaps = gst_caps_new_simple("video/x-raw",
                                     "format", G_TYPE_STRING, "I420",
                                     nullptr);
    }
    g_object_set(G_OBJECT(filter), "caps", rawCaps, nullptr);
    gst_caps_unref(rawCaps);

    const guint bitrate = targetWidth > 960 || targetHeight > 540
            ? 3000000u
            : 1600000u;
    g_object_set(G_OBJECT(encoder), "target-bitrate", bitrate, nullptr);

    if (g_object_class_find_property(G_OBJECT_GET_CLASS(encoder), "deadline")) {
        g_object_set(G_OBJECT(encoder), "deadline", gint64(1), nullptr);
    }
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(encoder), "cpu-used")) {
        g_object_set(G_OBJECT(encoder), "cpu-used", 8, nullptr);
    }
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(encoder), "lag-in-frames")) {
        g_object_set(G_OBJECT(encoder), "lag-in-frames", guint(0), nullptr);
    }
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(encoder), "keyframe-max-dist")) {
        g_object_set(G_OBJECT(encoder), "keyframe-max-dist", 50, nullptr);
    }
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(encoder), "threads")) {
        g_object_set(G_OBJECT(encoder), "threads", guint(4), nullptr);
    }

    gst_bin_add_many(GST_BIN(m_pipeline),
                     queue,
                     convert,
                     scale,
                     filter,
                     encoder,
                     outputQueue,
                     nullptr);

    const bool linked = gst_element_link_many(queue,
                                              convert,
                                              scale,
                                              filter,
                                              encoder,
                                              outputQueue,
                                              nullptr)
            && gst_element_link(outputQueue, m_mux);
    if (!linked) {
        return false;
    }

    syncWithParent(QList<GstElement *>()
                   << queue << convert << scale << filter << encoder << outputQueue);

    GstPad *queueSink = gst_element_get_static_pad(queue, "sink");
    if (!queueSink) {
        return false;
    }

    const GstPadLinkReturn result = gst_pad_link(pad, queueSink);
    gst_object_unref(queueSink);

    if (inputCaps) {
        gst_caps_unref(inputCaps);
    }

    if (result == GST_PAD_LINK_OK) {
        qInfo() << "SailVideo Cast transcode: VP8 target"
                << targetWidth << "x" << targetHeight
                << "bitrate" << bitrate;
        return true;
    }

    return false;
}

bool CastMediaPreparer::linkDecodedAudioPad(GstPad *pad)
{
    GstElement *queue = gst_element_factory_make("queue", nullptr);
    GstElement *convert = gst_element_factory_make("audioconvert", nullptr);
    GstElement *resample = gst_element_factory_make("audioresample", nullptr);
    GstElement *filter = gst_element_factory_make("capsfilter", nullptr);
    GstElement *encoder = gst_element_factory_make("vorbisenc", nullptr);
    GstElement *outputQueue = gst_element_factory_make("queue", nullptr);

    if (!queue || !convert || !resample || !filter || !encoder || !outputQueue) {
        if (queue) gst_object_unref(queue);
        if (convert) gst_object_unref(convert);
        if (resample) gst_object_unref(resample);
        if (filter) gst_object_unref(filter);
        if (encoder) gst_object_unref(encoder);
        if (outputQueue) gst_object_unref(outputQueue);
        return false;
    }

    // Downmix multichannel AVI audio to stereo and use a common 48 kHz rate.
    // This keeps the WebM output broadly compatible with Chromecast receivers.
    GstCaps *audioCaps = gst_caps_new_simple("audio/x-raw",
                                            "channels", G_TYPE_INT, 2,
                                            "rate", G_TYPE_INT, 48000,
                                            nullptr);
    g_object_set(G_OBJECT(filter), "caps", audioCaps, nullptr);
    gst_caps_unref(audioCaps);

    gst_bin_add_many(GST_BIN(m_pipeline),
                     queue,
                     convert,
                     resample,
                     filter,
                     encoder,
                     outputQueue,
                     nullptr);

    const bool linked = gst_element_link_many(queue,
                                              convert,
                                              resample,
                                              filter,
                                              encoder,
                                              outputQueue,
                                              nullptr)
            && gst_element_link(outputQueue, m_mux);
    if (!linked) {
        return false;
    }

    syncWithParent(QList<GstElement *>()
                   << queue << convert << resample << filter << encoder << outputQueue);

    GstPad *queueSink = gst_element_get_static_pad(queue, "sink");
    if (!queueSink) {
        return false;
    }

    const GstPadLinkReturn result = gst_pad_link(pad, queueSink);
    gst_object_unref(queueSink);
    return result == GST_PAD_LINK_OK;
}

bool CastMediaPreparer::discardPad(GstPad *pad)
{
    GstElement *sink = gst_element_factory_make("fakesink", nullptr);
    if (!sink) {
        return false;
    }

    g_object_set(G_OBJECT(sink), "sync", FALSE, "async", FALSE, nullptr);
    gst_bin_add(GST_BIN(m_pipeline), sink);
    gst_element_sync_state_with_parent(sink);

    GstPad *sinkPad = gst_element_get_static_pad(sink, "sink");
    if (!sinkPad) {
        return false;
    }

    const GstPadLinkReturn result = gst_pad_link(pad, sinkPad);
    gst_object_unref(sinkPad);
    return result == GST_PAD_LINK_OK;
}

void CastMediaPreparer::requestTranscodeFallback(const QString &reason)
{
    if (!m_busy.load() || m_mode.load() != RemuxMp4Mode) {
        return;
    }

    bool expected = false;
    if (!m_fallbackScheduled.compare_exchange_strong(expected, true)) {
        return;
    }

    QMetaObject::invokeMethod(this,
                              "startTranscodeFallback",
                              Qt::QueuedConnection,
                              Q_ARG(QString, reason));
}

void CastMediaPreparer::startTranscodeFallback(const QString &reason)
{
    if (!m_busy.load()) {
        return;
    }

    // If this slot is invoked while already transcoding, it represents a real
    // transcode-branch failure rather than a request to switch modes.
    if (m_mode.load() == TranscodeWebmMode) {
        finishFailure(reason);
        return;
    }

    if (m_mode.load() != RemuxMp4Mode) {
        return;
    }

    qInfo() << "SailVideo Cast remux: lossless MP4 copy not possible;"
            << reason;
    qInfo() << "SailVideo Cast transcode: restarting AVI preparation as VP8/Vorbis WebM";

    m_mode.store(NoPreparationMode);
    teardownPipeline();
    if (!m_partPath.isEmpty()) {
        QFile::remove(m_partPath);
    }

    m_videoLinked.store(false);
    m_audioLinked.store(false);
    m_fallbackScheduled.store(false);
    m_streamReadyEmitted = false;
    m_lastProgressBytes = 0;
    m_mode.store(TranscodeWebmMode);

    QString error;
    if (!startTranscodePipeline(&error)) {
        finishFailure(error);
        return;
    }

    emit transcodingStarted(m_sourceKey);
}

void CastMediaPreparer::pollBus()
{
    if (!m_busy.load() || !m_bus) {
        return;
    }

    // The lossless pipeline can post NOT_LINKED while a queued fallback request
    // is waiting for the Qt event loop. Let the fallback slot tear it down.
    if (m_mode.load() == RemuxMp4Mode && m_fallbackScheduled.load()) {
        return;
    }

    if (m_mode.load() == TranscodeWebmMode
            && m_sourceSeekPhase != SourceSeekComplete) {
        processTranscodeSourceSeek();
        if (!m_busy.load()) {
            return;
        }

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_sourceSeekPhase != SourceSeekComplete
                && now - m_lastDiagnosticOutputLogMs >= 1000) {
            m_lastDiagnosticOutputLogMs = now;
            logPipelineState("source-seek phase heartbeat");
        }
    }

    if (m_mode.load() == TranscodeWebmMode
            && m_sourceSeekPhase == SourceSeekComplete
            && !m_partPath.isEmpty()) {
        const QFileInfo growingFile(m_partPath);
        const qint64 bytes = growingFile.exists() ? growingFile.size() : 0;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        if (m_lastDiagnosticOutputBytes < 0
                || qAbs(bytes - m_lastDiagnosticOutputBytes) >= (256 * 1024)
                || now - m_lastDiagnosticOutputLogMs >= 1000) {
            qInfo() << diagnosticTag()
                    << "WebM output growth:" << bytes << "bytes;"
                    << "delta" << (m_lastDiagnosticOutputBytes < 0
                                   ? bytes
                                   : bytes - m_lastDiagnosticOutputBytes)
                    << "videoLinked" << m_videoLinked.load()
                    << "audioLinked" << m_audioLinked.load()
                    << "readyEmitted" << m_streamReadyEmitted;
            m_lastDiagnosticOutputBytes = bytes;
            m_lastDiagnosticOutputLogMs = now;
        }

        if (bytes >= m_lastProgressBytes + (1024 * 1024)) {
            m_lastProgressBytes = bytes;
            emit transcodeProgress(m_sourceKey, bytes);
        }

        // The Xperia 10 III test pipeline produced data comfortably faster
        // than playback. One MiB gets the receiver probing sooner while still
        // leaving a useful initial buffer.
        const qint64 StartBufferBytes = 1 * 1024 * 1024;
        if (!m_streamReadyEmitted
                && m_videoLinked.load()
                && bytes >= StartBufferBytes) {
            m_streamReadyEmitted = true;
            const QString fileUrl = QUrl::fromLocalFile(m_partPath).toString();
            qInfo() << diagnosticTag()
                    << "SailVideo Cast transcode: progressive WebM buffer ready"
                    << bytes << "bytes";
            emit transcodeStreamReady(m_sourceKey,
                                      fileUrl,
                                      QStringLiteral("video/webm"));
        }
    }

    for (;;) {
        GstMessage *message = gst_bus_pop_filtered(
                    m_bus,
                    static_cast<GstMessageType>(GST_MESSAGE_ERROR
                                                | GST_MESSAGE_WARNING
                                                | GST_MESSAGE_EOS
                                                | GST_MESSAGE_STATE_CHANGED
                                                | GST_MESSAGE_ASYNC_DONE
                                                | GST_MESSAGE_STREAM_START));
        if (!message) {
            break;
        }

        const GstMessageType messageType = GST_MESSAGE_TYPE(message);

        if (messageType == GST_MESSAGE_STATE_CHANGED) {
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(m_pipeline)) {
                GstState oldState = GST_STATE_VOID_PENDING;
                GstState newState = GST_STATE_VOID_PENDING;
                GstState pendingState = GST_STATE_VOID_PENDING;
                gst_message_parse_state_changed(message,
                                                &oldState,
                                                &newState,
                                                &pendingState);
                qInfo() << diagnosticTag()
                        << "GStreamer bus pipeline state"
                        << gst_element_state_get_name(oldState)
                        << "->" << gst_element_state_get_name(newState)
                        << "pending" << gst_element_state_get_name(pendingState);
            }
            gst_message_unref(message);
            continue;
        }

        if (messageType == GST_MESSAGE_ASYNC_DONE) {
            if (m_mode.load() == TranscodeWebmMode
                    && m_sourceSeekPhase == SourceSeekPostSeek
                    && GST_MESSAGE_SRC(message) == GST_OBJECT(m_pipeline)) {
                m_sourceSeekAsyncDone = true;
                qInfo() << diagnosticTag()
                        << "GStreamer bus post-seek ASYNC_DONE";
            } else {
                qInfo() << diagnosticTag() << "GStreamer bus ASYNC_DONE";
            }
            gst_message_unref(message);
            continue;
        }

        if (messageType == GST_MESSAGE_STREAM_START) {
            qInfo() << diagnosticTag() << "GStreamer bus STREAM_START";
            gst_message_unref(message);
            continue;
        }

        if (messageType == GST_MESSAGE_WARNING) {
            GError *warning = nullptr;
            gchar *debug = nullptr;
            gst_message_parse_warning(message, &warning, &debug);
            qWarning() << diagnosticTag()
                       << "GStreamer bus WARNING:"
                       << (warning && warning->message
                           ? QString::fromUtf8(warning->message)
                           : QStringLiteral("<no warning text>"));
            if (debug && *debug) {
                qWarning() << diagnosticTag()
                           << "GStreamer warning detail:"
                           << QString::fromUtf8(debug);
            }
            if (warning) g_error_free(warning);
            g_free(debug);
            gst_message_unref(message);
            continue;
        }

        if (messageType == GST_MESSAGE_ERROR) {
            GError *error = nullptr;
            gchar *debug = nullptr;
            gst_message_parse_error(message, &error, &debug);

            QString detail;
            if (error && error->message) {
                detail = QString::fromUtf8(error->message);
            }
            if (debug && *debug) {
                qWarning() << diagnosticTag()
                           << "SailVideo Cast preparation GStreamer detail:"
                           << QString::fromUtf8(debug);
            }

            if (error) g_error_free(error);
            g_free(debug);
            gst_message_unref(message);

            if (detail.contains(QStringLiteral("No space left"),
                                Qt::CaseInsensitive)) {
                finishFailure(tr("There is not enough free storage to prepare this AVI for Chromecast."));
            } else if (m_mode.load() == TranscodeWebmMode) {
                finishFailure(detail.isEmpty()
                              ? tr("This AVI could not be transcoded into a Chromecast-compatible WebM file.")
                              : tr("AVI transcoding failed: %1").arg(detail));
            } else {
                // A remux pipeline can fail for codec/container negotiation even
                // if the pad caps looked copy-compatible. Try the generic WebM
                // transcode path once before surfacing an error.
                requestTranscodeFallback(
                            detail.isEmpty()
                            ? tr("Lossless AVI remux failed.")
                            : detail);
            }
            return;
        }

        if (messageType == GST_MESSAGE_EOS) {
            qInfo() << diagnosticTag() << "GStreamer bus EOS";
            gst_message_unref(message);

            if (!m_videoLinked.load()) {
                finishFailure(m_mode.load() == TranscodeWebmMode
                              ? tr("The AVI video stream could not be decoded for transcoding.")
                              : tr("The AVI does not contain a Cast-compatible video stream."));
            } else {
                finishSuccess();
            }
            return;
        }

        gst_message_unref(message);
    }
}

void CastMediaPreparer::finishSuccess()
{
    qInfo() << diagnosticTag() << "preparation finished successfully;"
            << "finalBytes"
            << (m_partPath.isEmpty() ? 0 : QFileInfo(m_partPath).size());
    const QString sourceKey = m_sourceKey;
    const QString outputPath = m_outputPath;
    const QString partPath = m_partPath;
    const QString contentType = contentTypeForPath(outputPath);
    const PreparationMode finishedMode = static_cast<PreparationMode>(m_mode.load());

    setBusy(false);
    m_mode.store(NoPreparationMode);
    m_fallbackScheduled.store(false);
    teardownPipeline();

    const QFileInfo partInfo(partPath);
    if (!partInfo.exists() || !partInfo.isFile() || partInfo.size() <= 1024) {
        finishFailure(tr("AVI preparation did not produce a usable Chromecast media file."));
        return;
    }

    if (partPath != outputPath) {
        QFile::remove(outputPath);
        if (!QFile::rename(partPath, outputPath)) {
            finishFailure(tr("Could not finalise the temporary Chromecast media file."));
            return;
        }
    }

    if (m_timelineOffset == 0) {
        m_preparedFiles.insert(sourceKey, outputPath);
    }
    setLastError(QString());

    const QString fileUrl = QUrl::fromLocalFile(outputPath).toString();
    qInfo() << (finishedMode == TranscodeWebmMode
                ? "SailVideo Cast transcode: AVI prepared successfully as WebM"
                : "SailVideo Cast remux: AVI prepared successfully as MP4");
    emit ready(sourceKey, fileUrl, contentType);
}

void CastMediaPreparer::finishFailure(const QString &message)
{
    qWarning() << diagnosticTag() << "preparation failed:" << message
               << "partBytes"
               << (m_partPath.isEmpty() ? 0 : QFileInfo(m_partPath).size());
    logPipelineState("failure state");
    const QString sourceKey = m_sourceKey;

    setBusy(false);
    m_mode.store(NoPreparationMode);
    m_fallbackScheduled.store(false);
    teardownPipeline();
    if (!m_partPath.isEmpty()) {
        QFile::remove(m_partPath);
    }

    setLastError(message);
    emit failed(sourceKey, message);
}

void CastMediaPreparer::teardownPipeline()
{
    m_busTimer.stop();

    if (m_pipeline) {
        logPipelineState("teardown begin");
        const GstStateChangeReturn nullState =
                gst_element_set_state(m_pipeline, GST_STATE_NULL);
        qInfo() << diagnosticTag()
                << "pipeline NULL request return" << int(nullState);
        logPipelineState("after NULL request");
    }

    if (m_bus) {
        gst_object_unref(m_bus);
        m_bus = nullptr;
    }

    if (m_pipeline) {
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }

    releaseSourceSeekWarmupRefs();
    m_sourceSeekPhase = SourceSeekNone;
    m_sourceSeekPhaseStartedMs = 0;
    m_sourceSeekAsyncDone = false;

    m_source = nullptr;
    m_demux = nullptr;
    m_mux = nullptr;
    m_sink = nullptr;

    qInfo() << diagnosticTag() << "pipeline teardown complete";
}

void CastMediaPreparer::setBusy(bool busy)
{
    const bool previous = m_busy.exchange(busy);
    if (previous == busy) {
        return;
    }

    emit busyChanged();
}

void CastMediaPreparer::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
}
