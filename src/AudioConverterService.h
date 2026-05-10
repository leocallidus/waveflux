#ifndef AUDIOCONVERTERSERVICE_H
#define AUDIOCONVERTERSERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class TrackModel;
typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;
typedef struct _GstMessage GstMessage;

class AudioConverterService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString sourceFile READ sourceFile WRITE setSourceFile NOTIFY sourceFileChanged)
    Q_PROPERTY(QString outputFile READ outputFile WRITE setOutputFile NOTIFY outputFileChanged)
    Q_PROPERTY(QString format READ format WRITE setFormat NOTIFY formatChanged)
    Q_PROPERTY(int bitrate READ bitrate WRITE setBitrate NOTIFY bitrateChanged)
    Q_PROPERTY(int sampleRate READ sampleRate WRITE setSampleRate NOTIFY sampleRateChanged)
    Q_PROPERTY(QString channelMode READ channelMode WRITE setChannelMode NOTIFY channelModeChanged)
    Q_PROPERTY(double playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(int pitchSemitones READ pitchSemitones WRITE setPitchSemitones NOTIFY pitchSemitonesChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QVariantMap statusPresentation READ statusPresentation NOTIFY statusPresentationChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantMap errorPresentation READ errorPresentation NOTIFY errorPresentationChanged)
    Q_PROPERTY(bool overwriteExisting READ overwriteExisting WRITE setOverwriteExisting NOTIFY overwriteExistingChanged)
    Q_PROPERTY(QVariantList formatProfiles READ formatProfiles CONSTANT)
    Q_PROPERTY(QVariantMap currentFormatProfile READ currentFormatProfile NOTIFY formatChanged)
    Q_PROPERTY(QVariantMap preflight READ preflight NOTIFY preflightChanged)

public:
    struct FormatProfile {
        const char *id;
        const char *label;
        const char *extension;
        const char *containerLabel;
        const char *codecLabel;
        const char *gstreamerMuxer;
        const char *gstreamerEncoder;
        bool lossy;
        bool supportsBitrate;
        bool supportsSampleRate;
        bool supportsChannels;
        bool supportsCompressionLevel;
        int defaultBitrateKbps;
        int defaultSampleRateHz;
    };

    explicit AudioConverterService(QObject *parent = nullptr);

    void initialize(TrackModel *trackModel);

    QString sourceFile() const { return m_sourceFile; }
    QString outputFile() const { return m_outputFile; }
    QString format() const { return m_format; }
    int bitrate() const { return m_bitrate; }
    int sampleRate() const { return m_sampleRate; }
    QString channelMode() const { return m_channelMode; }
    double playbackRate() const { return m_playbackRate; }
    int pitchSemitones() const { return m_pitchSemitones; }
    bool isRunning() const { return m_isRunning; }
    double progress() const { return m_progress; }
    QString statusText() const { return m_statusText; }
    QVariantMap statusPresentation() const;
    QString lastError() const { return m_lastError; }
    QVariantMap errorPresentation() const;
    bool overwriteExisting() const { return m_overwriteExisting; }
    QVariantList formatProfiles() const;
    QVariantMap currentFormatProfile() const;
    QVariantMap lastConversionMetrics() const;
    QVariantMap preflight() const;

    Q_INVOKABLE bool startConversion();
    Q_INVOKABLE void cancelConversion();
    Q_INVOKABLE void resetTransientState();
    Q_INVOKABLE QString suggestOutputFilePath(const QString &directoryOverride = QString()) const;
    Q_INVOKABLE bool supportsCurrentFormatBitrate() const;
    Q_INVOKABLE bool supportsCurrentFormatSampleRate() const;
    Q_INVOKABLE bool supportsCurrentFormatChannels() const;
    Q_INVOKABLE bool outputFileExists(const QString &path = QString()) const;

public slots:
    void setSourceFile(const QString &sourceFile);
    void setOutputFile(const QString &outputFile);
    void setFormat(const QString &format);
    void setBitrate(int bitrate);
    void setSampleRate(int sampleRate);
    void setChannelMode(const QString &channelMode);
    void setPlaybackRate(double playbackRate);
    void setPitchSemitones(int pitchSemitones);
    void setOverwriteExisting(bool overwriteExisting);

signals:
    void sourceFileChanged();
    void outputFileChanged();
    void formatChanged();
    void bitrateChanged();
    void sampleRateChanged();
    void channelModeChanged();
    void playbackRateChanged();
    void pitchSemitonesChanged();
    void isRunningChanged();
    void progressChanged();
    void statusTextChanged();
    void statusPresentationChanged();
    void lastErrorChanged();
    void errorPresentationChanged();
    void overwriteExistingChanged();
    void preflightChanged();
    void conversionStarted();
    void conversionFinished(const QString &outputPath);
    void conversionFailed(const QString &message);
    void conversionCanceled();

private:
    static QString normalizeFormat(const QString &format);
    static QString normalizeChannelMode(const QString &channelMode);
    static int normalizeBitrateForFormat(int bitrate, const QString &format);
    static int normalizeSampleRate(int sampleRate, const QString &format);
    static double normalizePlaybackRate(double playbackRate);
    static int normalizePitchSemitones(int pitchSemitones);
    static const FormatProfile *findFormatProfile(const QString &format);
    static QVariantMap toVariantMap(const FormatProfile &profile);
    static QString replaceExtension(const QString &path, const QString &extension);
    static QString uniqueOutputPath(const QString &path);
    static QStringList requiredGStreamerElements(const FormatProfile *profile);
    static QStringList missingGStreamerElements(const QStringList &requiredElements);
    static bool hasGStreamerElementFactory(const QString &factoryName);

    QVariantMap buildPreflight() const;
    static QString preflightMessageText(const QVariantMap &preflight);
    QString validateForStart() const;
    void resetLastConversionMetrics();
    void finalizeLastConversionMetrics(const QString &terminationKey);
    void setLastError(const QString &message);
    void setStatusText(const QString &text);
    void setStatusPresentation(const QString &messageKey,
                               const QVariantList &messageArgs = QVariantList(),
                               const QString &fallbackText = QString());
    void setErrorPresentation(const QString &messageKey,
                              const QVariantList &messageArgs = QVariantList(),
                              const QString &fallbackText = QString());
    void setProgress(double progress);
    void setIsRunning(bool running);
    void teardownConversionPipeline();
    void handleBusMessage(GstMessage *message);
    bool setupConversionPipeline(QString *errorMessage);
    QString createTemporaryOutputPath() const;
    QString createTemporaryTrackerRenderPath() const;
    void finalizeSuccessfulConversion();
    void failConversion(const QString &messageKey,
                        const QVariantList &messageArgs = QVariantList(),
                        const QString &terminationKey = QStringLiteral("runtime-failed"),
                        const QString &diagnosticMessage = QString());
    void updateProgressFromPipeline();
    bool refreshSourceDurationFromPipeline();
    bool copyBasicSourceTagsToOutput(const QString &outputPath, QString *warningMessage);
    bool prepareTrackerSourceForConversion(QString *pipelineSourcePath, QString *errorMessage);
    void cleanupTemporaryTrackerSource();

    TrackModel *m_trackModel = nullptr;
    QString m_sourceFile;
    QString m_outputFile;
    QString m_format = QStringLiteral("mp3");
    int m_bitrate = 320;
    int m_sampleRate = 44100;
    QString m_channelMode = QStringLiteral("stereo");
    double m_playbackRate = 1.0;
    int m_pitchSemitones = 0;
    bool m_isRunning = false;
    double m_progress = 0.0;
    QString m_statusText;
    QString m_statusMessageKey;
    QVariantList m_statusMessageArgs;
    QString m_lastError;
    QString m_errorMessageKey;
    QVariantList m_errorMessageArgs;
    GstElement *m_pipeline = nullptr;
    GstElement *m_decodeBin = nullptr;
    GstElement *m_convertElement = nullptr;
    GstElement *m_resampleElement = nullptr;
    GstElement *m_pitchElement = nullptr;
    GstElement *m_postConvertElement = nullptr;
    GstElement *m_capsFilterElement = nullptr;
    GstElement *m_encoderElement = nullptr;
    GstElement *m_muxerElement = nullptr;
    GstElement *m_sinkElement = nullptr;
    GstBus *m_bus = nullptr;
    QTimer m_busPollTimer;
    QTimer m_progressTimer;
    QString m_pendingTrackerRenderFile;
    QString m_pendingTempOutputFile;
    QString m_pendingFinalOutputFile;
    qint64 m_sourceDurationMs = 0;
    bool m_cancelRequested = false;
    bool m_completionHandled = false;
    bool m_overwriteExisting = false;
    qint64 m_lastConversionStartedAtMs = 0;
    qint64 m_lastConversionFinishedAtMs = 0;
    qint64 m_lastConversionWallClockMs = 0;
    qint64 m_lastConversionSourceBytes = 0;
    qint64 m_lastConversionTempBytes = 0;
    qint64 m_lastConversionFinalBytes = 0;
    bool m_lastConversionUsedTemporaryFile = false;
    bool m_lastConversionMetadataCopyAttempted = false;
    bool m_lastConversionMetadataCopySucceeded = false;
    qint64 m_lastConversionMetadataCopyDurationUs = 0;
    QString m_lastConversionTerminationKey = QStringLiteral("not-started");
};

#endif // AUDIOCONVERTERSERVICE_H
