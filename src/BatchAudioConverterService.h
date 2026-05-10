#ifndef BATCHAUDIOCONVERTERSERVICE_H
#define BATCHAUDIOCONVERTERSERVICE_H

#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class AudioConverterService;

class BatchAudioConverterService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QVariantMap currentItem READ currentItem NOTIFY itemsChanged)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap jobMetadata READ jobMetadata NOTIFY jobMetadataChanged)
    Q_PROPERTY(QVariantList finishedJobHistory READ finishedJobHistory NOTIFY finishedJobHistoryChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(bool cancelRequested READ cancelRequested NOTIFY cancelRequestedChanged)
    Q_PROPERTY(double batchProgress READ batchProgress NOTIFY batchProgressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString reportExportError READ reportExportError NOTIFY reportExportErrorChanged)
    Q_PROPERTY(bool hasFinished READ hasFinished NOTIFY finalSummaryChanged)
    Q_PROPERTY(bool wasCanceled READ wasCanceled NOTIFY finalSummaryChanged)
    Q_PROPERTY(QVariantMap finalSummary READ finalSummary NOTIFY finalSummaryChanged)
    Q_PROPERTY(QString outputDirectory READ outputDirectory WRITE setOutputDirectory NOTIFY outputDirectoryChanged)
    Q_PROPERTY(QString namingPolicy READ namingPolicy WRITE setNamingPolicy NOTIFY namingPolicyChanged)
    Q_PROPERTY(QString format READ format WRITE setFormat NOTIFY formatChanged)
    Q_PROPERTY(QString conflictPolicy READ conflictPolicy WRITE setConflictPolicy NOTIFY conflictPolicyChanged)
    Q_PROPERTY(QString retryPolicy READ retryPolicy WRITE setRetryPolicy NOTIFY retryPolicyChanged)
    Q_PROPERTY(QString playlistAddMode READ playlistAddMode WRITE setPlaylistAddMode NOTIFY playlistAddModeChanged)
    Q_PROPERTY(int bitrate READ bitrate WRITE setBitrate NOTIFY bitrateChanged)
    Q_PROPERTY(int sampleRate READ sampleRate WRITE setSampleRate NOTIFY sampleRateChanged)
    Q_PROPERTY(QString channelMode READ channelMode WRITE setChannelMode NOTIFY channelModeChanged)
    Q_PROPERTY(double playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(int pitchSemitones READ pitchSemitones WRITE setPitchSemitones NOTIFY pitchSemitonesChanged)
    Q_PROPERTY(bool addResultsToPlaylist READ addResultsToPlaylist WRITE setAddResultsToPlaylist NOTIFY addResultsToPlaylistChanged)
    Q_PROPERTY(bool canAddSucceededResultsToPlaylist READ canAddSucceededResultsToPlaylist NOTIFY finalSummaryChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY itemsChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY itemsChanged)
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY itemsChanged)
    Q_PROPERTY(int runningCount READ runningCount NOTIFY itemsChanged)
    Q_PROPERTY(int succeededCount READ succeededCount NOTIFY itemsChanged)
    Q_PROPERTY(int failedCount READ failedCount NOTIFY itemsChanged)
    Q_PROPERTY(int canceledCount READ canceledCount NOTIFY itemsChanged)
    Q_PROPERTY(int skippedCount READ skippedCount NOTIFY itemsChanged)

public:
    enum ItemState {
        Pending,
        Running,
        Succeeded,
        Failed,
        Canceled,
        Skipped
    };
    Q_ENUM(ItemState)

    enum SourceOriginType {
        UnknownSourceOrigin,
        PlaylistSelectionSourceOrigin,
        FilePickerSourceOrigin,
        FolderImportSourceOrigin,
        DroppedUrlSourceOrigin
    };
    Q_ENUM(SourceOriginType)

    enum ItemActionability {
        NoActionability,
        PendingActionability,
        RunningActionability,
        RetryableActionability,
        TerminalActionability
    };
    Q_ENUM(ItemActionability)

    enum FailureType {
        NoFailure,
        ValidationFailure,
        SourceMissingFailure,
        OutputConflictFailure,
        UnsupportedFormatFailure,
        EncoderUnavailableFailure,
        PermissionDeniedFailure,
        InternalPipelineFailure,
        BackendFailure,
        CanceledFailure
    };
    Q_ENUM(FailureType)

    enum ConflictPolicy {
        AutoRenameConflictPolicy,
        OverwriteExistingConflictPolicy,
        SkipOnConflictPolicy,
        FailOnConflictPolicy
    };
    Q_ENUM(ConflictPolicy)

    enum RetryPolicy {
        ManualRetryPolicy,
        RetryFailedOnlyPolicy,
        RetryFailedAndSkippedPolicy
    };
    Q_ENUM(RetryPolicy)

    enum PlaylistAddMode {
        ImmediatePlaylistAddMode,
        DeferredPlaylistAddMode,
        DisabledPlaylistAddMode
    };
    Q_ENUM(PlaylistAddMode)

    struct BatchAudioConversionSettings {
        QString outputDirectory;
        QString namingPolicy = QStringLiteral("basename");
        QString format = QStringLiteral("mp3");
        ConflictPolicy conflictPolicy = AutoRenameConflictPolicy;
        RetryPolicy retryPolicy = ManualRetryPolicy;
        PlaylistAddMode playlistAddMode = ImmediatePlaylistAddMode;
        int bitrate = 320;
        int sampleRate = 44100;
        QString channelMode = QStringLiteral("stereo");
        double playbackRate = 1.0;
        int pitchSemitones = 0;
        bool addResultsToPlaylist = true;
    };

    struct EffectiveSettingsSnapshot {
        QString outputDirectory;
        QString namingPolicy;
        QString format;
        ConflictPolicy conflictPolicy = AutoRenameConflictPolicy;
        RetryPolicy retryPolicy = ManualRetryPolicy;
        PlaylistAddMode playlistAddMode = ImmediatePlaylistAddMode;
        int bitrate = 0;
        int sampleRate = 0;
        QString channelMode;
        double playbackRate = 1.0;
        int pitchSemitones = 0;
        bool addResultsToPlaylist = true;
        qint64 capturedAtMs = 0;
    };

    struct ConflictResolutionInfo {
        QString requestedOutputFile;
        QString resolvedOutputFile;
        QString resolutionKey = QStringLiteral("none");
        QString collisionRuleKey = QStringLiteral("none");
        bool hadConflict = false;
        bool willOverwriteExisting = false;
        bool targetExistsOnDisk = false;
        QString finalizationStrategyKey = QStringLiteral("temp-commit");
    };

    struct PreviewNamingDecision {
        QString requestedNamingPolicy;
        QString appliedNamingPolicy;
        QString fallbackNamingPolicy;
        QString baseName;
        QString sourceDirectoryPolicy;
        QStringList missingMetadataFields;
        bool usedFallback = false;
    };

    struct PlannedOutputInfo {
        QString outputFile;
        ItemState state = Pending;
        FailureType failureType = NoFailure;
        QString statusText;
        QString errorText;
        ConflictResolutionInfo conflictResolution;
        QVariantMap previewDiagnostics;
        bool runnable = false;
    };

    struct BatchAudioConversionItem {
        QString itemId;
        QString sourceFile;
        QString sourceDisplayName;
        QString sourceFormat;
        qint64 sourceDurationMs = 0;
        SourceOriginType sourceOriginType = UnknownSourceOrigin;
        QString outputFile;
        ItemState state = Pending;
        double progress = 0.0;
        QString statusText;
        QString errorText;
        QString resultFile;
        int retryCount = 0;
        qint64 createdAtMs = 0;
        qint64 updatedAtMs = 0;
        QString terminalResult = QStringLiteral("none");
        FailureType failureType = NoFailure;
        EffectiveSettingsSnapshot effectiveSettings;
        ConflictResolutionInfo conflictResolution;
        QVariantMap reportMetadata;
    };

    explicit BatchAudioConverterService(QObject *parent = nullptr);

    QVariantList items() const;
    QVariantMap currentItem() const;
    QVariantMap settings() const;
    QVariantMap jobMetadata() const;
    QVariantList finishedJobHistory() const { return m_finishedJobHistory; }
    QVariantMap runtimeDiagnostics() const;
    QVariantMap parallelismDecision() const;

    QString outputDirectory() const { return m_settings.outputDirectory; }
    QString namingPolicy() const { return m_settings.namingPolicy; }
    QString format() const { return m_settings.format; }
    QString conflictPolicy() const;
    QString retryPolicy() const;
    QString playlistAddMode() const;
    int bitrate() const { return m_settings.bitrate; }
    int sampleRate() const { return m_settings.sampleRate; }
    QString channelMode() const { return m_settings.channelMode; }
    double playbackRate() const { return m_settings.playbackRate; }
    int pitchSemitones() const { return m_settings.pitchSemitones; }
    bool addResultsToPlaylist() const { return m_settings.addResultsToPlaylist; }
    bool canAddSucceededResultsToPlaylist() const;
    bool isRunning() const { return m_isRunning; }
    bool cancelRequested() const { return m_cancelRequested; }
    double batchProgress() const { return m_batchProgress; }
    QString statusText() const { return m_statusText; }
    QString lastError() const { return m_lastError; }
    QString reportExportError() const { return m_reportExportError; }
    bool hasFinished() const { return m_hasFinished; }
    bool wasCanceled() const { return m_wasCanceled; }
    QVariantMap finalSummary() const;

    int totalCount() const { return m_items.size(); }
    int currentIndex() const;
    int pendingCount() const;
    int runningCount() const;
    int succeededCount() const;
    int failedCount() const;
    int canceledCount() const;
    int skippedCount() const;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void setSourceFiles(const QStringList &sourceFiles);
    Q_INVOKABLE void setSourceFilesFromVariantList(const QVariantList &sourceFiles);
    Q_INVOKABLE QVariantMap replaceSourceFilesFromVariantList(const QVariantList &sourceFiles,
                                                              const QString &sourceOriginType);
    Q_INVOKABLE QVariantMap appendSourceFilesFromVariantList(const QVariantList &sourceFiles,
                                                             const QString &sourceOriginType);
    Q_INVOKABLE QVariantMap replaceSourceFolder(const QString &folderPath);
    Q_INVOKABLE QVariantMap appendSourceFolder(const QString &folderPath);
    Q_INVOKABLE bool startBatch();
    Q_INVOKABLE void cancelBatch();
    Q_INVOKABLE QVariantMap exportDraftState() const;
    Q_INVOKABLE bool restoreDraftState(const QVariantMap &draftState);
    Q_INVOKABLE QVariantMap currentReport() const;
    Q_INVOKABLE bool exportCurrentReportToFile(const QString &filePath, const QString &format);
    Q_INVOKABLE bool exportHistoryReportToFile(const QString &jobId,
                                               const QString &filePath,
                                               const QString &format);
    Q_INVOKABLE QString currentReportText(const QString &format = QStringLiteral("txt")) const;
    Q_INVOKABLE bool replaceFinishedJobHistory(const QVariantList &history);
    Q_INVOKABLE QString suggestedReportFileName(const QString &format) const;
    Q_INVOKABLE QVariantList succeededResultFiles() const;
    Q_INVOKABLE int addSucceededResultsToPlaylist();
    Q_INVOKABLE QVariantMap exportPresetSettings() const;
    Q_INVOKABLE bool applySettingsMap(const QVariantMap &settings);
    Q_INVOKABLE QString itemIdAt(int index) const;
    Q_INVOKABLE int indexOfItemId(const QString &itemId) const;
    Q_INVOKABLE QVariantMap itemById(const QString &itemId) const;
    Q_INVOKABLE bool canRemoveItem(const QString &itemId) const;
    Q_INVOKABLE bool canRetryItem(const QString &itemId) const;
    Q_INVOKABLE bool canMoveItemUp(const QString &itemId) const;
    Q_INVOKABLE bool canMoveItemDown(const QString &itemId) const;
    Q_INVOKABLE bool removeItemById(const QString &itemId);
    Q_INVOKABLE int removeItemsById(const QVariantList &itemIds);
    Q_INVOKABLE int clearFailedItems();
    Q_INVOKABLE int clearCompletedItems();
    Q_INVOKABLE bool retryItemById(const QString &itemId);
    Q_INVOKABLE int retryItemsById(const QVariantList &itemIds);
    Q_INVOKABLE int retryFailedItems();
    Q_INVOKABLE int retrySkippedItems();
    Q_INVOKABLE bool moveItemUp(const QString &itemId);
    Q_INVOKABLE bool moveItemDown(const QString &itemId);
    Q_INVOKABLE bool setItemState(int index, ItemState state);
    Q_INVOKABLE bool setItemStateById(const QString &itemId, ItemState state);
    Q_INVOKABLE bool setItemProgress(int index, double progress);
    Q_INVOKABLE bool setItemProgressById(const QString &itemId, double progress);
    Q_INVOKABLE bool setItemStatusText(int index, const QString &statusText);
    Q_INVOKABLE bool setItemErrorText(int index, const QString &errorText);
    Q_INVOKABLE bool setItemOutputFile(int index, const QString &outputFile);
    Q_INVOKABLE bool setItemResultFile(int index, const QString &resultFile);
    Q_INVOKABLE bool setItemSourceMetadata(int index,
                                           const QString &sourceDisplayName,
                                           const QString &sourceFormat,
                                           qint64 sourceDurationMs);

public slots:
    void setOutputDirectory(const QString &outputDirectory);
    void setNamingPolicy(const QString &namingPolicy);
    void setFormat(const QString &format);
    void setConflictPolicy(const QString &conflictPolicy);
    void setRetryPolicy(const QString &retryPolicy);
    void setPlaylistAddMode(const QString &playlistAddMode);
    void setBitrate(int bitrate);
    void setSampleRate(int sampleRate);
    void setChannelMode(const QString &channelMode);
    void setPlaybackRate(double playbackRate);
    void setPitchSemitones(int pitchSemitones);
    void setAddResultsToPlaylist(bool addResultsToPlaylist);

signals:
    void itemsChanged();
    void settingsChanged();
    void jobMetadataChanged();
    void isRunningChanged();
    void cancelRequestedChanged();
    void batchProgressChanged();
    void statusTextChanged();
    void lastErrorChanged();
    void reportExportErrorChanged();
    void finalSummaryChanged();
    void outputDirectoryChanged();
    void namingPolicyChanged();
    void formatChanged();
    void conflictPolicyChanged();
    void retryPolicyChanged();
    void playlistAddModeChanged();
    void bitrateChanged();
    void sampleRateChanged();
    void channelModeChanged();
    void playbackRateChanged();
    void pitchSemitonesChanged();
    void addResultsToPlaylistChanged();
    void batchStarted();
    void batchFinished();
    void batchCanceled();
    void playlistResultReady(const QString &outputPath);
    void finishedJobHistoryChanged();

private:
    static QString normalizeLocalPath(const QString &path);
    static QString inferDisplayName(const QString &path);
    static QString inferFormat(const QString &path);
    static QString normalizeSourceFormat(const QString &format);
    static SourceOriginType normalizeSourceOriginType(const QString &sourceOriginType);
    static QString normalizeNamingPolicy(const QString &namingPolicy);
    static QString normalizeFormat(const QString &format);
    static ConflictPolicy normalizeConflictPolicy(const QString &conflictPolicy);
    static RetryPolicy normalizeRetryPolicy(const QString &retryPolicy);
    static PlaylistAddMode normalizePlaylistAddMode(const QString &playlistAddMode,
                                                    bool addResultsToPlaylistFallback = true);
    static QString normalizeChannelMode(const QString &channelMode);
    static int normalizeBitrate(int bitrate);
    static int normalizeSampleRate(int sampleRate);
    static double normalizePlaybackRate(double playbackRate);
    static int normalizePitchSemitones(int pitchSemitones);
    static QString uniqueOutputPath(const QString &path,
                                    const QSet<QString> &reservedPaths,
                                    const QSet<QString> &existingPaths);
    static QString sanitizeFileNameComponent(const QString &value);
    static QString itemStateKey(ItemState state);
    static QString sourceOriginTypeKey(SourceOriginType sourceOriginType);
    static QString itemActionabilityKey(ItemActionability actionability);
    static QString failureTypeKey(FailureType failureType);
    static QString conflictPolicyKey(ConflictPolicy conflictPolicy);
    static QString retryPolicyKey(RetryPolicy retryPolicy);
    static QString playlistAddModeKey(PlaylistAddMode playlistAddMode);
    static QVariantMap effectiveSettingsToVariantMap(const EffectiveSettingsSnapshot &settings);
    static EffectiveSettingsSnapshot effectiveSettingsFromVariantMap(const QVariantMap &settings);
    static QVariantMap conflictResolutionToVariantMap(const ConflictResolutionInfo &info);
    static ConflictResolutionInfo conflictResolutionFromVariantMap(const QVariantMap &info);
    static QVariantMap toVariantMap(const BatchAudioConversionItem &item);
    static QVariantList stringListToVariantList(const QStringList &values);
    static QStringList settingsDiffKeys(const QVariantMap &previousSettings,
                                        const QVariantMap &currentSettings);
    static QString newIdentity();
    static qint64 nowMs();
    static bool isTerminalState(ItemState state);
    static bool isRetryEligibleState(ItemState state);
    static bool isRetryEligibleFailureType(FailureType failureType);
    static FailureType classifyFailureMessage(const QString &message, FailureType fallbackFailureType);
    static void recordTerminalAttempt(BatchAudioConversionItem &item);
    static QString csvEscape(const QString &value);
    static bool isTrackerSourceItem(const BatchAudioConversionItem &item);
    static qint64 currentProcessCpuTimeMs();
    static qint64 currentPeakResidentMemoryKb();

    bool hasItemAt(int index) const;
    int indexOfItemIdInternal(const QString &itemId) const;
    int countByState(ItemState state) const;
    bool canMutateConfiguration() const;
    bool canRemoveItemAt(int index) const;
    bool canRetryItemAt(int index) const;
    bool canRetryItemAt(int index, ItemState expectedState) const;
    bool canMoveItemAt(int index, int delta) const;
    void prepareItemForRetry(BatchAudioConversionItem &item);
    void resetSummaryForQueueMutation();
    bool moveItemInternal(int from, int to);
    void setIsRunning(bool running);
    void setCancelRequested(bool cancelRequested);
    void setBatchProgress(double batchProgress);
    void setStatusText(const QString &statusText);
    void setLastError(const QString &lastError);
    void setReportExportError(const QString &errorText);
    void resetFinalSummary();
    void setFinalSummaryState(bool hasFinished, bool wasCanceled);
    void resetJobMetadata();
    void beginNewJobSession();
    void setJobStartedNow();
    void setJobFinishedNow();
    void touchItem(BatchAudioConversionItem &item);
    void applyTerminalState(BatchAudioConversionItem &item, ItemState state, FailureType failureType);
    void resetRuntimeMeasurements();
    void beginRuntimeMeasurements();
    void refreshRuntimeMeasurementPeaks();
    void absorbWorkerMetrics(const BatchAudioConversionItem &item, const QVariantMap &workerMetrics);
    EffectiveSettingsSnapshot currentSettingsSnapshot() const;
    QVariantMap buildReportForCurrentJob() const;
    QVariantMap buildReportSummaryFromMap(const QVariantMap &report) const;
    QVariantMap serializeDraftItem(const BatchAudioConversionItem &item) const;
    bool parseDraftItem(const QVariantMap &serialized, BatchAudioConversionItem *itemOut) const;
    QVariantMap sanitizeDraftState(const QVariantMap &draftState) const;
    void appendFinishedJobReport(const QVariantMap &report);
    QVariantMap finishedJobReportById(const QString &jobId) const;
    bool exportReportMapToFile(const QVariantMap &report,
                               const QString &filePath,
                               const QString &format);
    QString reportAsPlainText(const QVariantMap &report) const;
    QString reportAsCsv(const QVariantMap &report) const;
    bool reportAsJson(const QVariantMap &report, QByteArray *jsonOut) const;
    QVariantMap ingestSources(const QStringList &sourceFiles,
                              SourceOriginType sourceOriginType,
                              bool append);
    QVariantMap ingestSourceFolder(const QString &folderPath, bool append);
    PreviewNamingDecision previewNamingDecisionForItem(const BatchAudioConversionItem &item) const;
    QString previewBaseNameForItem(const BatchAudioConversionItem &item) const;
    QString artistTitleBaseNameForItem(const QString &sourceFile,
                                       QStringList *missingMetadataFieldsOut = nullptr) const;
    QString albumTrackTitleBaseNameForItem(const QString &sourceFile,
                                           QStringList *missingMetadataFieldsOut = nullptr) const;
    bool refreshPlannedOutputs(QString *errorMessage);
    bool prepareItemsForBatchStart(QString *errorMessage);
    PlannedOutputInfo planOutputForItem(const BatchAudioConversionItem &item,
                                        const PreviewNamingDecision &namingDecision,
                                        const QSet<QString> &reservedPaths,
                                        const QSet<QString> &existingPaths) const;
    void startNextPendingItem();
    void finalizeBatchRun(bool canceled);
    void updateAggregateProgress();
    void markPendingItemsAsCanceled();
    void emitSettingsChanged();

    BatchAudioConversionSettings m_settings;
    QList<BatchAudioConversionItem> m_items;
    QPointer<AudioConverterService> m_worker;
    bool m_isRunning = false;
    bool m_cancelRequested = false;
    double m_batchProgress = 0.0;
    QString m_statusText;
    QString m_lastError;
    QString m_reportExportError;
    bool m_hasFinished = false;
    bool m_wasCanceled = false;
    QVariantList m_finishedJobHistory;
    QString m_jobId;
    qint64 m_jobCreatedAtMs = 0;
    qint64 m_jobStartedAtMs = 0;
    qint64 m_jobFinishedAtMs = 0;
    qint64 m_runtimeMeasurementStartedAtMs = 0;
    qint64 m_runtimeMeasurementCpuStartedAtMs = 0;
    qint64 m_runtimeMeasurementCpuFinishedAtMs = 0;
    qint64 m_runtimePeakResidentMemoryKb = -1;
    qint64 m_runtimeMeasuredSourceBytes = 0;
    qint64 m_runtimeMeasuredResultBytes = 0;
    qint64 m_runtimePeakTempBytes = 0;
    int m_runtimePeakTempFiles = 0;
    int m_runtimeMeasuredItemCount = 0;
    int m_runtimeMaxConcurrentJobsObserved = 0;
    int m_runtimeTagCopyAttemptCount = 0;
    int m_runtimeTagCopySuccessCount = 0;
    qint64 m_runtimeTagCopyTotalDurationUs = 0;
    bool m_deferredPlaylistResultsAdded = false;
};

Q_DECLARE_METATYPE(BatchAudioConverterService::ItemState)
Q_DECLARE_METATYPE(BatchAudioConverterService::SourceOriginType)
Q_DECLARE_METATYPE(BatchAudioConverterService::ItemActionability)
Q_DECLARE_METATYPE(BatchAudioConverterService::FailureType)
Q_DECLARE_METATYPE(BatchAudioConverterService::ConflictPolicy)
Q_DECLARE_METATYPE(BatchAudioConverterService::RetryPolicy)
Q_DECLARE_METATYPE(BatchAudioConverterService::PlaylistAddMode)

#endif // BATCHAUDIOCONVERTERSERVICE_H
