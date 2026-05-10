#ifndef YTDLPIMPORTSERVICE_H
#define YTDLPIMPORTSERVICE_H

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class AppSettingsManager;
class QProcess;

class YtDlpImportService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString sourceUrl READ sourceUrl WRITE setSourceUrl NOTIFY sourceUrlChanged)
    Q_PROPERTY(bool isProbing READ isProbing NOTIFY isProbingChanged)
    Q_PROPERTY(QString outputDirectory READ outputDirectory WRITE setOutputDirectory NOTIFY outputDirectoryChanged)
    Q_PROPERTY(QString selectedFormat READ selectedFormat WRITE setSelectedFormat NOTIFY selectedFormatChanged)
    Q_PROPERTY(QString namingPolicy READ namingPolicy WRITE setNamingPolicy NOTIFY namingPolicyChanged)
    Q_PROPERTY(QString conflictPolicy READ conflictPolicy WRITE setConflictPolicy NOTIFY conflictPolicyChanged)
    Q_PROPERTY(int parallelDownloads READ parallelDownloads WRITE setParallelDownloads NOTIFY parallelDownloadsChanged)
    Q_PROPERTY(QVariantList recentSourceUrls READ recentSourceUrls NOTIFY recentSourceUrlsChanged)
    Q_PROPERTY(QVariantList recentCanonicalSourceUrls READ recentCanonicalSourceUrls NOTIFY recentCanonicalSourceUrlsChanged)
    Q_PROPERTY(QVariantList recentOutputDirectories READ recentOutputDirectories NOTIFY recentOutputDirectoriesChanged)
    Q_PROPERTY(QVariantMap importJob READ importJob NOTIFY importJobChanged)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(bool cancelRequested READ cancelRequested NOTIFY cancelRequestedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantMap probeResult READ probeResult NOTIFY probeResultChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY probeResultChanged)
    Q_PROPERTY(bool hasProbeResult READ hasProbeResult NOTIFY probeResultChanged)
    Q_PROPERTY(bool hasUnavailableEntries READ hasUnavailableEntries NOTIFY probeResultChanged)
    Q_PROPERTY(double batchProgress READ batchProgress NOTIFY batchProgressChanged)
    Q_PROPERTY(QVariantMap finalSummary READ finalSummary NOTIFY finalSummaryChanged)
    Q_PROPERTY(QVariantList completedReports READ completedReports NOTIFY completedReportsChanged)

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

    enum SourceStatus {
        SourcePendingProbe,
        SourceProbing,
        SourceReady,
        SourceReadyWithIssues,
        SourceProbeFailed,
        SourceImporting,
        SourceCompleted,
        SourceCompletedWithFailures,
        SourceCanceled
    };
    Q_ENUM(SourceStatus)

    enum ItemFailureType {
        NoFailure,
        ContentFailure,
        NetworkFailure,
        DependencyFailure,
        PermissionFailure,
        DiskFailure,
        OutputFailure,
        PostprocessFailure,
        CanceledFailure,
        GenericFailure
    };
    Q_ENUM(ItemFailureType)

    enum RetryEligibility {
        RetryNotApplicable,
        RetryAllowed,
        RetryBlocked
    };
    Q_ENUM(RetryEligibility)

    enum PersistenceEligibility {
        NotPersistable,
        DraftPersistable,
        ReportPersistable
    };
    Q_ENUM(PersistenceEligibility)

    struct ProbeEntry {
        QString sourceUrl;
        QString extractor;
        QString title;
        QString entryId;
        int playlistIndex = -1;
        qint64 duration = 0;
        QString thumbnail;
        QString webpageUrl;
        QString availability;
        bool isPlayable = false;
        int metadataOrder = -1;
    };

    struct ProbeResult {
        QString sourceUrl;
        QString resolvedSourceUrl;
        QString extractor;
        QString title;
        QString playlistTitle;
        QString playlistId;
        QList<ProbeEntry> entries;
        bool isPlaylist = false;
        bool hasUnavailableEntries = false;
        bool isRedirected = false;
    };

    struct ImportSource {
        QString sourceId;
        qint64 createdAtMs = 0;
        qint64 lastProbedAtMs = 0;
        SourceStatus status = SourcePendingProbe;
        ItemFailureType failureType = NoFailure;
        RetryEligibility retryEligibility = RetryNotApplicable;
        PersistenceEligibility persistenceEligibility = DraftPersistable;
        QVariantMap immutableSourceInput;
        QVariantMap metadataSnapshot;
        QVariantMap queueMetadata;
        QVariantMap runtimeState;
        QVariantMap finalResultState;
    };

    struct ImportEntry {
        struct ConflictResolutionInfo {
            QString requestedOutputFile;
            QString resolvedOutputFile;
            QString resolutionKey = QStringLiteral("none");
            QString collisionRuleKey = QStringLiteral("none");
            bool hadConflict = false;
            bool targetExistsOnDisk = false;
            QString finalizationStrategyKey = QStringLiteral("temp-commit");
        };

        QString entryId;
        QString sourceId;
        QString sourceUrl;
        QString extractor;
        QString title;
        QString entrySourceId;
        qint64 duration = 0;
        QString thumbnail;
        QString webpageUrl;
        QString availability;
        int playlistIndex = -1;
        ItemState state = Pending;
        double progress = 0.0;
        QString statusText;
        QString errorText;
        QString plannedOutputFile;
        QString finalOutputFile;
        int resultTrackInsertIndex = -1;
        int metadataOrder = -1;
        bool isPlayable = false;
        QString stagingDirectory;
        QString stagingOutputFile;
        QVariantMap previewDiagnostics;
        QVariantMap immutableSourceInput;
        QVariantMap metadataSnapshot;
        QVariantMap queueMetadata;
        QVariantMap runtimeState;
        QVariantMap finalResultState;
        int retryCount = 0;
        qint64 createdAtMs = 0;
        qint64 updatedAtMs = 0;
        ItemFailureType failureType = NoFailure;
        RetryEligibility retryEligibility = RetryNotApplicable;
        PersistenceEligibility persistenceEligibility = DraftPersistable;
        ConflictResolutionInfo conflictResolution;
    };

    using ImportItem = ImportEntry;

    struct ImportJob {
        QString jobId;
        qint64 createdAtMs = 0;
        qint64 startedAtMs = 0;
        qint64 finishedAtMs = 0;
        PersistenceEligibility persistenceEligibility = DraftPersistable;
        QVariantMap defaultsSnapshot;
    };

    explicit YtDlpImportService(QObject *parent = nullptr);
    ~YtDlpImportService() override;

    QString sourceUrl() const { return m_sourceUrl; }
    bool isProbing() const { return m_isProbing; }
    QString outputDirectory() const { return m_outputDirectory; }
    QString selectedFormat() const { return m_selectedFormat; }
    QString namingPolicy() const { return m_namingPolicy; }
    QString conflictPolicy() const { return m_conflictPolicy; }
    int parallelDownloads() const { return m_parallelDownloads; }
    QVariantList recentSourceUrls() const { return m_recentSourceUrls; }
    QVariantList recentCanonicalSourceUrls() const { return m_recentCanonicalSourceUrls; }
    QVariantList recentOutputDirectories() const { return m_recentOutputDirectories; }
    QVariantMap importJob() const;
    QVariantList sources() const;
    QVariantList items() const;
    bool isRunning() const { return m_isRunning; }
    bool cancelRequested() const { return m_cancelRequested; }
    QString statusText() const { return m_statusText; }
    QString lastError() const { return m_lastError; }
    QVariantMap probeResult() const { return m_probeResult; }
    QVariantList entries() const;
    bool hasProbeResult() const { return m_hasProbeResult; }
    bool hasUnavailableEntries() const { return m_probeResultData.hasUnavailableEntries; }
    double batchProgress() const { return m_batchProgress; }
    QVariantMap finalSummary() const { return m_finalSummary; }
    QVariantList completedReports() const { return m_completedReports; }

    void setAppSettingsManager(AppSettingsManager *settingsManager);

    static bool parseProbeJson(const QByteArray &payload,
                               const QString &sourceUrl,
                               ProbeResult *outResult,
                               QString *outErrorMessage = nullptr);
    static QVariantMap importJobToVariantMap(const ImportJob &job);
    static QVariantMap importSourceToVariantMap(const ImportSource &source);
    static QVariantMap probeResultToVariantMap(const ProbeResult &result);
    static QVariantMap probeEntryToVariantMap(const ProbeEntry &entry);
    static QVariantMap conflictResolutionToVariantMap(const ImportItem::ConflictResolutionInfo &info);
    static QVariantMap importItemToVariantMap(const ImportItem &item);

public slots:
    void setSourceUrl(const QString &sourceUrl);
    void setOutputDirectory(const QString &outputDirectory);
    void setSelectedFormat(const QString &selectedFormat);
    void setNamingPolicy(const QString &namingPolicy);
    void setConflictPolicy(const QString &conflictPolicy);
    void setParallelDownloads(int parallelDownloads);

    Q_INVOKABLE bool probeSource();
    Q_INVOKABLE bool probeSourceUrl(const QString &sourceUrl);
    Q_INVOKABLE bool probeSourceById(const QString &sourceId);
    Q_INVOKABLE bool probeAllSources();
    Q_INVOKABLE bool probeFailedOrStaleSources();
    Q_INVOKABLE bool startImport();
    Q_INVOKABLE void cancelProbe();
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE void clear();
    Q_INVOKABLE QVariantMap replaceSourceUrl(const QString &sourceUrl);
    Q_INVOKABLE QVariantMap appendSourceUrl(const QString &sourceUrl);
    Q_INVOKABLE QVariantMap replaceSourcesFromText(const QString &sourceText);
    Q_INVOKABLE QVariantMap appendSourcesFromText(const QString &sourceText);
    Q_INVOKABLE QVariantMap replaceSourcesFromVariantList(const QVariantList &sourceUrls);
    Q_INVOKABLE QVariantMap appendSourcesFromVariantList(const QVariantList &sourceUrls);
    Q_INVOKABLE bool canRemoveSource(const QString &sourceId) const;
    Q_INVOKABLE bool canMoveSourceUp(const QString &sourceId) const;
    Q_INVOKABLE bool canMoveSourceDown(const QString &sourceId) const;
    Q_INVOKABLE bool removeSourceById(const QString &sourceId);
    Q_INVOKABLE int removeSourcesById(const QVariantList &sourceIds);
    Q_INVOKABLE int clearFailedProbes();
    Q_INVOKABLE int clearCompletedImports();
    Q_INVOKABLE int retryFailedProbes();
    Q_INVOKABLE int retryFailedImports();
    Q_INVOKABLE int retrySelectedItemsById(const QVariantList &itemIds);
    Q_INVOKABLE bool moveSourceUp(const QString &sourceId);
    Q_INVOKABLE bool moveSourceDown(const QString &sourceId);
    Q_INVOKABLE QVariantMap exportCurrentJobState() const;
    Q_INVOKABLE QVariantMap currentSettingsPreset() const;
    Q_INVOKABLE bool applySettingsPreset(const QVariantMap &preset);
    Q_INVOKABLE QVariantMap latestCompletedReport() const;
    Q_INVOKABLE bool reopenCompletedReport(const QString &jobId);
    Q_INVOKABLE QString currentReportText() const;
    Q_INVOKABLE QString sourceIdAt(int index) const;
    Q_INVOKABLE int indexOfSourceId(const QString &sourceId) const;
    Q_INVOKABLE QVariantMap sourceById(const QString &sourceId) const;
    Q_INVOKABLE QString itemIdAt(int index) const;
    Q_INVOKABLE int indexOfItemId(const QString &itemId) const;
    Q_INVOKABLE QVariantMap itemById(const QString &itemId) const;

signals:
    void sourceUrlChanged();
    void isProbingChanged();
    void outputDirectoryChanged();
    void selectedFormatChanged();
    void namingPolicyChanged();
    void conflictPolicyChanged();
    void parallelDownloadsChanged();
    void recentSourceUrlsChanged();
    void recentCanonicalSourceUrlsChanged();
    void recentOutputDirectoriesChanged();
    void importJobChanged();
    void sourcesChanged();
    void itemsChanged();
    void isRunningChanged();
    void cancelRequestedChanged();
    void statusTextChanged();
    void lastErrorChanged();
    void probeResultChanged();
    void batchProgressChanged();
    void finalSummaryChanged();
    void completedReportsChanged();
    void playlistImportReady(const QStringList &filePaths);

private:
    void clearProbeResult();
    void clearImportSession();
    void clearQueueState();
    void cleanupStagingRootDirectory();
    void finalizeImportProcess(QProcess *process);
    void cleanupItemArtifacts(ImportItem &item);
    bool finalizeSuccessfulImport(ImportItem &item);
    void finishImportWithError(const QString &itemId, const QString &message);
    void finalizeImportRun(bool wasCanceled);
    bool prepareImportQueue();
    bool scheduleImportWorkers();
    bool canStartMoreWorkers() const;
    int nextPendingItemIndexForRun() const;
    bool startNextImportItem();
    void requestImportProcessCancellation(const QString &itemId, QProcess *process);
    void markPendingItemsCanceled(int runSerial = -1);
    void updateBatchProgress();
    void updateImportRuntimeStatusText();
    void setBatchProgress(double batchProgress);
    void setCancelRequested(bool cancelRequested);
    void setIsRunning(bool isRunning);
    void setFinalSummary(const QVariantMap &summary);
    void processImportOutputChunk(const QString &itemId, const QByteArray &chunk, bool isStdErr);
    void setStatusText(const QString &statusText);
    void setLastError(const QString &lastError);
    void finishProbeWithError(const QString &message);
    void finalizeProcess(QProcess *process);
    void resetJobLifecycle();
    void refreshSourceQueuePositions();
    void invalidateSingleSourceProbeView();
    bool enqueueProbeSources(const QStringList &sourceIds);
    bool startNextProbeSource();
    bool startProbeProcessForSource(int sourceIndex);
    void completeProbeSuccess(int sourceIndex, const ProbeResult &result);
    void completeProbeFailure(int sourceIndex, const QString &message);
    bool sourceNeedsProbe(const ImportSource &source) const;
    bool sourceNeedsRetryProbe(const ImportSource &source) const;
    int activeProbeSourceIndex() const;
    void rebuildPreviewItemsFromSources();
    QList<int> itemIndexesForSource(const QString &sourceId) const;
    bool sourceHasRunningEntry(const QString &sourceId) const;
    bool sourceHasOnlyPendingTailEntries(const QString &sourceId) const;
    QString activeImportItemIdForProcess(const QProcess *process) const;
    int activeImportItemIndex(const QString &itemId) const;
    int furthestActiveImportItemIndex() const;
    void registerActiveImportProcess(const QString &itemId, int itemIndex, QProcess *process);
    void unregisterActiveImportProcess(const QString &itemId);
    void archiveItemAttempt(ImportItem &item, const QString &reasonKey);
    void publishSources();
    void publishImportJob();
    void publishItems(bool force = false);
    void syncEntryRuntimeLayers(ImportItem &item);
    void syncSourceRuntimeFromItems();
    QVariantMap applySourceIntake(const QStringList &rawInputs, bool replaceExistingSources);
    void reloadPersistedHistory();
    void persistSettingsSnapshot();
    void persistDraftState();
    QVariantMap buildDraftStateForPersistence() const;
    bool restorePersistedDraft(const QVariantMap &draft);
    void persistRecentSourceUrl(const QString &url);
    void persistRecentCanonicalSourceUrl(const QString &url);
    void persistRecentOutputDirectory(const QString &directory);
    void clearUnstartedOutputPlan();
    void refreshPreviewOutputPlan();
    void archiveCompletedReport(const QVariantMap &summary);
    QString reportTextForSummary(const QVariantMap &summary) const;

    QPointer<AppSettingsManager> m_appSettingsManager;
    QPointer<QProcess> m_probeProcess;
    QString m_sourceUrl;
    QString m_outputDirectory;
    QString m_selectedFormat = QStringLiteral("mp3");
    QString m_namingPolicy = QStringLiteral("auto");
    QString m_conflictPolicy = QStringLiteral("auto-rename");
    int m_parallelDownloads = 1;
    QString m_statusText;
    QString m_lastError;
    ProbeResult m_probeResultData;
    ImportJob m_importJob;
    QList<ImportSource> m_importSources;
    QList<ImportItem> m_importItems;
    QVariantMap m_probeResult;
    QVariantMap m_finalSummary;
    bool m_hasProbeResult = false;
    bool m_isProbing = false;
    bool m_isRunning = false;
    bool m_cancelRequested = false;
    double m_batchProgress = 0.0;
    QByteArray m_probeStderrBuffer;
    QByteArray m_probeStderrLogBuffer;
    QHash<QString, QPointer<QProcess>> m_activeImportProcesses;
    QHash<QString, int> m_activeImportItemIndexes;
    QHash<QString, QByteArray> m_importStdoutBuffers;
    QHash<QString, QByteArray> m_importStderrLineBuffers;
    QHash<QString, QByteArray> m_importStderrBuffers;
    QSet<QString> m_cancelPendingItemIds;
    bool m_queuePrepared = false;
    QStringList m_probeQueueSourceIds;
    QString m_currentProbeSourceId;
    int m_activeRunSerial = 0;
    QVariantList m_recentSourceUrls;
    QVariantList m_recentCanonicalSourceUrls;
    QVariantList m_recentOutputDirectories;
    QVariantList m_completedReports;
    bool m_restoringPersistedDraft = false;
};

#endif // YTDLPIMPORTSERVICE_H
