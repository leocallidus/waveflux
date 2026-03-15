#include "WindowsMediaControlsService.h"

#include <atomic>
#include <iterator>

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QUrl>
#include <QWindow>
#include <QtGlobal>

#include "AudioEngine.h"
#include "PlaybackController.h"
#include "TrackModel.h"

#ifdef Q_OS_WIN
#include <initguid.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>
#include <roapi.h>
#include <shcore.h>
#include <shellapi.h>
#include <windows.foundation.h>
#include <systemmediatransportcontrolsinterop.h>
#include <winstring.h>
#include <windows.media.h>
#include <windows.storage.streams.h>
#endif

#ifdef Q_OS_WIN
using ButtonPressedHandlerInterface = ABI::Windows::Foundation::ITypedEventHandler<
    ABI::Windows::Media::SystemMediaTransportControls *,
    ABI::Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs *>;

namespace {
bool smtcDiagEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("WAVEFLUX_SMTC_DIAG");
    return enabled;
}

void smtcDiag(const QString &message)
{
    if (!smtcDiagEnabled()) {
        return;
    }

    qInfo().noquote() << "[SMTCDiag]" << message;
}

template <typename T>
void releaseCom(T *&pointer)
{
    if (pointer) {
        pointer->Release();
        pointer = nullptr;
    }
}

class ScopedHString
{
public:
    ~ScopedHString()
    {
        if (m_value) {
            WindowsDeleteString(m_value);
        }
    }

    bool assign(const QString &value)
    {
        if (m_value) {
            WindowsDeleteString(m_value);
            m_value = nullptr;
        }

        const std::wstring wide = value.toStdWString();
        return SUCCEEDED(WindowsCreateString(wide.c_str(),
                                             static_cast<UINT32>(wide.size()),
                                             &m_value));
    }

    bool assignLiteral(const wchar_t *value)
    {
        if (m_value) {
            WindowsDeleteString(m_value);
            m_value = nullptr;
        }

        return SUCCEEDED(WindowsCreateString(value,
                                             static_cast<UINT32>(wcslen(value)),
                                             &m_value));
    }

    HSTRING get() const { return m_value; }

private:
    HSTRING m_value = nullptr;
};

bool hasLoadedPlaybackContext(const WindowsMediaControlsService::MetadataSnapshot &metadata)
{
    return !metadata.filePath.isEmpty();
}

bool metadataSnapshotsEqual(const WindowsMediaControlsService::MetadataSnapshot &lhs,
                            const WindowsMediaControlsService::MetadataSnapshot &rhs)
{
    return lhs.filePath == rhs.filePath
        && lhs.title == rhs.title
        && lhs.artist == rhs.artist
        && lhs.album == rhs.album
        && lhs.artworkUrl == rhs.artworkUrl;
}

bool capabilitiesSnapshotsEqual(const WindowsMediaControlsService::CapabilitiesSnapshot &lhs,
                                const WindowsMediaControlsService::CapabilitiesSnapshot &rhs)
{
    return lhs.canPlay == rhs.canPlay
        && lhs.canPause == rhs.canPause
        && lhs.canGoNext == rhs.canGoNext
        && lhs.canGoPrevious == rhs.canGoPrevious
        && lhs.canSeek == rhs.canSeek;
}

ABI::Windows::Foundation::TimeSpan timeSpanFromMs(qint64 valueMs)
{
    ABI::Windows::Foundation::TimeSpan span {};
    span.Duration = qMax<qint64>(0, valueMs) * 10000;
    return span;
}

ABI::Windows::Media::MediaPlaybackStatus mapPlaybackStatus(
    const WindowsMediaControlsService::PlaybackSnapshot &playback,
    const WindowsMediaControlsService::MetadataSnapshot &metadata)
{
    if (!hasLoadedPlaybackContext(metadata)) {
        return ABI::Windows::Media::MediaPlaybackStatus_Closed;
    }

    switch (playback.state) {
    case AudioEngine::PlayingState:
        return ABI::Windows::Media::MediaPlaybackStatus_Playing;
    case AudioEngine::PausedState:
        return ABI::Windows::Media::MediaPlaybackStatus_Paused;
    case AudioEngine::ReadyState:
        return ABI::Windows::Media::MediaPlaybackStatus_Changing;
    case AudioEngine::EndedState:
    case AudioEngine::StoppedState:
    case AudioEngine::ErrorState:
    default:
        return ABI::Windows::Media::MediaPlaybackStatus_Stopped;
    }
}

QString playbackStatusString(ABI::Windows::Media::MediaPlaybackStatus status)
{
    switch (status) {
    case ABI::Windows::Media::MediaPlaybackStatus_Closed:
        return QStringLiteral("Closed");
    case ABI::Windows::Media::MediaPlaybackStatus_Changing:
        return QStringLiteral("Changing");
    case ABI::Windows::Media::MediaPlaybackStatus_Stopped:
        return QStringLiteral("Stopped");
    case ABI::Windows::Media::MediaPlaybackStatus_Playing:
        return QStringLiteral("Playing");
    case ABI::Windows::Media::MediaPlaybackStatus_Paused:
        return QStringLiteral("Paused");
    default:
        return QStringLiteral("Unknown(%1)").arg(static_cast<int>(status));
    }
}

QString buttonName(ABI::Windows::Media::SystemMediaTransportControlsButton button)
{
    switch (button) {
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Play:
        return QStringLiteral("Play");
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Pause:
        return QStringLiteral("Pause");
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Stop:
        return QStringLiteral("Stop");
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Next:
        return QStringLiteral("Next");
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Previous:
        return QStringLiteral("Previous");
    default:
        return QStringLiteral("Other(%1)").arg(static_cast<int>(button));
    }
}

QString metadataDisplayTitle(const WindowsMediaControlsService::MetadataSnapshot &metadata)
{
    if (!metadata.title.trimmed().isEmpty()) {
        return metadata.title.trimmed();
    }

    if (!metadata.filePath.trimmed().isEmpty()) {
        return QFileInfo(metadata.filePath).completeBaseName();
    }

    return QString();
}

QString windowsAppUserModelId()
{
    return QStringLiteral("WaveFlux.Desktop");
}

QString windowsRelaunchDisplayName()
{
    return QStringLiteral("WaveFlux");
}

bool setWindowStringProperty(HWND hwnd, REFPROPERTYKEY key, const QString &value)
{
    if (!hwnd || value.isEmpty()) {
        return false;
    }

    IPropertyStore *propertyStore = nullptr;
    const HRESULT storeHr = SHGetPropertyStoreForWindow(hwnd,
                                                        IID_PPV_ARGS(&propertyStore));
    if (FAILED(storeHr) || !propertyStore) {
        releaseCom(propertyStore);
        return false;
    }

    PROPVARIANT propVar;
    PropVariantInit(&propVar);
    const std::wstring wideValue = value.toStdWString();
    const HRESULT initHr = InitPropVariantFromString(wideValue.c_str(), &propVar);
    if (FAILED(initHr)) {
        releaseCom(propertyStore);
        PropVariantClear(&propVar);
        return false;
    }

    const HRESULT setHr = propertyStore->SetValue(key, propVar);
    HRESULT commitHr = S_OK;
    if (SUCCEEDED(setHr)) {
        commitHr = propertyStore->Commit();
    }

    PropVariantClear(&propVar);
    releaseCom(propertyStore);
    return SUCCEEDED(setHr) && SUCCEEDED(commitHr);
}

void applyWindowShellIdentity(HWND hwnd)
{
    if (!hwnd) {
        return;
    }

    const QString appId = windowsAppUserModelId();
    const QString displayName = windowsRelaunchDisplayName();
    const QString relaunchCommand =
        QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    const QString iconResource =
        QStringLiteral("%1,0").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));

    const bool appIdOk = setWindowStringProperty(hwnd, PKEY_AppUserModel_ID, appId);
    const bool commandOk = setWindowStringProperty(hwnd, PKEY_AppUserModel_RelaunchCommand, relaunchCommand);
    const bool nameOk = setWindowStringProperty(hwnd,
                                                PKEY_AppUserModel_RelaunchDisplayNameResource,
                                                displayName);
    const bool iconOk = setWindowStringProperty(hwnd,
                                                PKEY_AppUserModel_RelaunchIconResource,
                                                iconResource);

    if (smtcDiagEnabled()) {
        smtcDiag(QStringLiteral("window identity applied appId=%1 relaunchCommand=%2 appIdOk=%3 commandOk=%4 nameOk=%5 iconOk=%6")
                     .arg(appId,
                          relaunchCommand,
                          appIdOk ? QStringLiteral("yes") : QStringLiteral("no"),
                          commandOk ? QStringLiteral("yes") : QStringLiteral("no"),
                          nameOk ? QStringLiteral("yes") : QStringLiteral("no"),
                          iconOk ? QStringLiteral("yes") : QStringLiteral("no")));
    }
}

bool createWinRtUri(const QString &uriString,
                    ABI::Windows::Foundation::IUriRuntimeClass **uri)
{
    if (!uri) {
        return false;
    }

    *uri = nullptr;

    ScopedHString className;
    ScopedHString rawUri;
    if (!className.assignLiteral(RuntimeClass_Windows_Foundation_Uri)
        || !rawUri.assign(uriString)) {
        return false;
    }

    ABI::Windows::Foundation::IUriRuntimeClassFactory *uriFactory = nullptr;
    const HRESULT factoryHr = RoGetActivationFactory(
        className.get(),
        IID___x_ABI_CWindows_CFoundation_CIUriRuntimeClassFactory,
        reinterpret_cast<void **>(&uriFactory));
    if (FAILED(factoryHr) || !uriFactory) {
        releaseCom(uriFactory);
        return false;
    }

    const HRESULT createHr = uriFactory->CreateUri(rawUri.get(), uri);
    releaseCom(uriFactory);
    return SUCCEEDED(createHr) && *uri;
}

bool createStreamReferenceForArtwork(const QString &artworkUrl,
                                     ABI::Windows::Storage::Streams::IRandomAccessStreamReference **streamReference)
{
    if (!streamReference) {
        return false;
    }

    *streamReference = nullptr;

    if (artworkUrl.trimmed().isEmpty()) {
        smtcDiag(QStringLiteral("artwork publish skipped reason=no-artwork"));
        return false;
    }

    const QUrl url(artworkUrl);
    if (!url.isValid() || url.scheme().compare(QStringLiteral("data"), Qt::CaseInsensitive) == 0) {
        smtcDiag(QStringLiteral("artwork publish skipped reason=invalid-or-data-url url=%1").arg(artworkUrl));
        return false;
    }

    QString uriString;
    if (url.isLocalFile()) {
        const QString localPath = url.toLocalFile();
        if (localPath.isEmpty() || !QFileInfo::exists(localPath)) {
            smtcDiag(QStringLiteral("artwork publish skipped reason=missing-local-file path=%1").arg(localPath));
            return false;
        }

        ScopedHString className;
        if (!className.assignLiteral(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference)) {
            smtcDiag(QStringLiteral("artwork publish failed stage=create-class-name"));
            return false;
        }

        ABI::Windows::Storage::Streams::IRandomAccessStreamReferenceStatics *streamReferenceStatics = nullptr;
        const HRESULT factoryHr = RoGetActivationFactory(
            className.get(),
            IID___x_ABI_CWindows_CStorage_CStreams_CIRandomAccessStreamReferenceStatics,
            reinterpret_cast<void **>(&streamReferenceStatics));
        if (FAILED(factoryHr) || !streamReferenceStatics) {
            releaseCom(streamReferenceStatics);
            smtcDiag(QStringLiteral("artwork publish failed stage=get-stream-reference-factory hr=0x%1")
                         .arg(QString::number(static_cast<qulonglong>(factoryHr), 16)));
            return false;
        }

        ABI::Windows::Storage::Streams::IRandomAccessStream *randomAccessStream = nullptr;
        const std::wstring widePath = QDir::toNativeSeparators(localPath).toStdWString();
        const HRESULT streamHr = CreateRandomAccessStreamOnFile(
            widePath.c_str(),
            GENERIC_READ,
            IID___x_ABI_CWindows_CStorage_CStreams_CIRandomAccessStream,
            reinterpret_cast<void **>(&randomAccessStream));
        if (FAILED(streamHr) || !randomAccessStream) {
            releaseCom(streamReferenceStatics);
            releaseCom(randomAccessStream);
            smtcDiag(QStringLiteral("artwork publish failed stage=create-random-access-stream hr=0x%1 path=%2")
                         .arg(QString::number(static_cast<qulonglong>(streamHr), 16),
                              localPath));
            return false;
        }

        const HRESULT createHr = streamReferenceStatics->CreateFromStream(randomAccessStream, streamReference);
        releaseCom(randomAccessStream);
        releaseCom(streamReferenceStatics);
        if (FAILED(createHr) || !*streamReference) {
            smtcDiag(QStringLiteral("artwork publish failed stage=create-from-stream hr=0x%1 path=%2")
                         .arg(QString::number(static_cast<qulonglong>(createHr), 16),
                              localPath));
        }
        return SUCCEEDED(createHr) && *streamReference;
    } else {
        uriString = url.toString(QUrl::FullyEncoded);
    }

    ABI::Windows::Foundation::IUriRuntimeClass *uri = nullptr;
    if (!createWinRtUri(uriString, &uri)) {
        smtcDiag(QStringLiteral("artwork publish failed stage=create-uri url=%1").arg(uriString));
        return false;
    }

    ScopedHString className;
    if (!className.assignLiteral(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference)) {
        releaseCom(uri);
        smtcDiag(QStringLiteral("artwork publish failed stage=create-class-name"));
        return false;
    }

    ABI::Windows::Storage::Streams::IRandomAccessStreamReferenceStatics *streamReferenceStatics = nullptr;
    const HRESULT factoryHr = RoGetActivationFactory(
        className.get(),
        IID___x_ABI_CWindows_CStorage_CStreams_CIRandomAccessStreamReferenceStatics,
        reinterpret_cast<void **>(&streamReferenceStatics));
    if (FAILED(factoryHr) || !streamReferenceStatics) {
        releaseCom(streamReferenceStatics);
        releaseCom(uri);
        smtcDiag(QStringLiteral("artwork publish failed stage=get-stream-reference-factory hr=0x%1")
                     .arg(QString::number(static_cast<qulonglong>(factoryHr), 16)));
        return false;
    }

    const HRESULT createHr = streamReferenceStatics->CreateFromUri(uri, streamReference);
    releaseCom(streamReferenceStatics);
    releaseCom(uri);
    if (FAILED(createHr) || !*streamReference) {
        smtcDiag(QStringLiteral("artwork publish failed stage=create-from-uri hr=0x%1 uri=%2")
                     .arg(QString::number(static_cast<qulonglong>(createHr), 16), uriString));
    }
    return SUCCEEDED(createHr) && *streamReference;
}

void publishDisplayMetadata(
    ABI::Windows::Media::ISystemMediaTransportControls *transportControls,
    const WindowsMediaControlsService::MetadataSnapshot &metadata)
{
    if (!transportControls) {
        return;
    }

    ABI::Windows::Media::ISystemMediaTransportControlsDisplayUpdater *displayUpdater = nullptr;
    HRESULT hr = transportControls->get_DisplayUpdater(&displayUpdater);
    if (FAILED(hr) || !displayUpdater) {
        releaseCom(displayUpdater);
        return;
    }

    displayUpdater->ClearAll();

    if (!hasLoadedPlaybackContext(metadata)) {
        smtcDiag(QStringLiteral("metadata publish cleared"));
        displayUpdater->Update();
        releaseCom(displayUpdater);
        return;
    }

    displayUpdater->put_Type(ABI::Windows::Media::MediaPlaybackType_Music);

    ScopedHString appMediaId;
    if (appMediaId.assign(metadata.filePath)) {
        displayUpdater->put_AppMediaId(appMediaId.get());
    }

    ABI::Windows::Media::IMusicDisplayProperties *musicProperties = nullptr;
    hr = displayUpdater->get_MusicProperties(&musicProperties);
    if (SUCCEEDED(hr) && musicProperties) {
        const QString title = metadataDisplayTitle(metadata);
        ScopedHString titleString;
        if (titleString.assign(title)) {
            musicProperties->put_Title(titleString.get());
        }

        ScopedHString artistString;
        if (artistString.assign(metadata.artist.trimmed())) {
            musicProperties->put_Artist(artistString.get());
            musicProperties->put_AlbumArtist(artistString.get());
        }

        ABI::Windows::Media::IMusicDisplayProperties2 *musicProperties2 = nullptr;
        if (SUCCEEDED(musicProperties->QueryInterface(IID___x_ABI_CWindows_CMedia_CIMusicDisplayProperties2,
                                                      reinterpret_cast<void **>(&musicProperties2)))
            && musicProperties2) {
            ScopedHString albumString;
            if (albumString.assign(metadata.album.trimmed())) {
                musicProperties2->put_AlbumTitle(albumString.get());
            }
        }
        releaseCom(musicProperties2);
    }
    releaseCom(musicProperties);

    ABI::Windows::Storage::Streams::IRandomAccessStreamReference *thumbnail = nullptr;
    const bool hasThumbnailReference = createStreamReferenceForArtwork(metadata.artworkUrl, &thumbnail);
    bool hasThumbnail = false;
    if (hasThumbnailReference) {
        const HRESULT thumbnailHr = displayUpdater->put_Thumbnail(thumbnail);
        hasThumbnail = SUCCEEDED(thumbnailHr);
        if (FAILED(thumbnailHr)) {
            smtcDiag(QStringLiteral("artwork publish failed stage=put-thumbnail hr=0x%1 file=%2")
                         .arg(QString::number(static_cast<qulonglong>(thumbnailHr), 16),
                              metadata.filePath));
        }
    }
    releaseCom(thumbnail);

    smtcDiag(QStringLiteral("metadata publish title=\"%1\" artist=\"%2\" album=\"%3\" artwork=%4 file=\"%5\"")
                 .arg(metadataDisplayTitle(metadata),
                      metadata.artist,
                      metadata.album,
                      hasThumbnail ? QStringLiteral("yes") : QStringLiteral("no"),
                      metadata.filePath));
    const HRESULT updateHr = displayUpdater->Update();
    if (FAILED(updateHr)) {
        smtcDiag(QStringLiteral("metadata publish failed stage=display-updater-update hr=0x%1 file=%2")
                     .arg(QString::number(static_cast<qulonglong>(updateHr), 16),
                          metadata.filePath));
    }
    releaseCom(displayUpdater);
}

bool createTimelineProperties(
    ABI::Windows::Media::ISystemMediaTransportControlsTimelineProperties **timelineProperties)
{
    if (!timelineProperties) {
        return false;
    }

    *timelineProperties = nullptr;

    ScopedHString className;
    if (!className.assignLiteral(RuntimeClass_Windows_Media_SystemMediaTransportControlsTimelineProperties)) {
        return false;
    }

    IInspectable *inspectable = nullptr;
    const HRESULT activateHr = RoActivateInstance(className.get(), &inspectable);
    if (FAILED(activateHr) || !inspectable) {
        releaseCom(inspectable);
        return false;
    }

    const HRESULT queryHr = inspectable->QueryInterface(
        IID___x_ABI_CWindows_CMedia_CISystemMediaTransportControlsTimelineProperties,
        reinterpret_cast<void **>(timelineProperties));
    releaseCom(inspectable);
    return SUCCEEDED(queryHr) && *timelineProperties;
}

void publishTimelineState(
    ABI::Windows::Media::ISystemMediaTransportControls2 *transportControls2,
    const WindowsMediaControlsService::PlaybackSnapshot &playback,
    const WindowsMediaControlsService::MetadataSnapshot &metadata)
{
    if (!transportControls2) {
        return;
    }

    ABI::Windows::Media::ISystemMediaTransportControlsTimelineProperties *timelineProperties = nullptr;
    if (!createTimelineProperties(&timelineProperties)) {
        return;
    }

    const bool hasTrackContext = hasLoadedPlaybackContext(metadata);
    const qint64 positionMs = qMax<qint64>(0, playback.positionMs);
    const qint64 durationMs = qMax<qint64>(0, playback.durationMs);
    const qint64 endTimeMs = hasTrackContext ? qMax(durationMs, positionMs) : 0;
    const qint64 boundedPositionMs = qBound<qint64>(0, positionMs, endTimeMs);
    const ABI::Windows::Foundation::TimeSpan zeroTime = timeSpanFromMs(0);
    const ABI::Windows::Foundation::TimeSpan positionTime = timeSpanFromMs(boundedPositionMs);
    const ABI::Windows::Foundation::TimeSpan endTime = timeSpanFromMs(endTimeMs);

    timelineProperties->put_StartTime(zeroTime);
    timelineProperties->put_EndTime(endTime);
    timelineProperties->put_Position(positionTime);

    // Stage 5 intentionally publishes a read-only timeline: progress is visible,
    // but system-side seek isn't advertised until request handling is implemented.
    timelineProperties->put_MinSeekTime(positionTime);
    timelineProperties->put_MaxSeekTime(positionTime);

    smtcDiag(QStringLiteral("timeline update active=%1 positionMs=%2 durationMs=%3 endMs=%4")
                 .arg(hasTrackContext ? QStringLiteral("yes") : QStringLiteral("no"))
                 .arg(boundedPositionMs)
                 .arg(durationMs)
                 .arg(endTimeMs));
    transportControls2->UpdateTimelineProperties(timelineProperties);
    releaseCom(timelineProperties);
}
}

class WindowsMediaButtonPressedHandler final : public ButtonPressedHandlerInterface
{
public:
    explicit WindowsMediaButtonPressedHandler(WindowsMediaControlsService *service)
        : m_service(service)
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
    {
        if (!ppvObject) {
            return E_POINTER;
        }

        *ppvObject = nullptr;
        if (InlineIsEqualGUID(riid, IID_IUnknown) ||
            InlineIsEqualGUID(riid, IID___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CSystemMediaTransportControlsButtonPressedEventArgs)) {
            *ppvObject = static_cast<ButtonPressedHandlerInterface *>(this);
            AddRef();
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return static_cast<ULONG>(++m_refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG remaining = static_cast<ULONG>(--m_refCount);
        if (remaining == 0) {
            delete this;
        }
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ABI::Windows::Media::ISystemMediaTransportControls *,
                                     ABI::Windows::Media::ISystemMediaTransportControlsButtonPressedEventArgs *args) override
    {
        if (!m_service || !args) {
            return S_OK;
        }

        ABI::Windows::Media::SystemMediaTransportControlsButton button =
            ABI::Windows::Media::SystemMediaTransportControlsButton_Play;
        const HRESULT hr = args->get_Button(&button);
        if (FAILED(hr)) {
            return hr;
        }

        const QPointer<WindowsMediaControlsService> service(m_service);
        QMetaObject::invokeMethod(m_service.data(),
                                  [service, button]() {
                                      if (!service) {
                                          return;
                                      }
                                      service->dispatchButtonPressed(static_cast<int>(button));
                                  },
                                  Qt::QueuedConnection);
        return S_OK;
    }

private:
    std::atomic_ulong m_refCount {1};
    QPointer<WindowsMediaControlsService> m_service;
};
#endif

WindowsMediaControlsService::WindowsMediaControlsService(AudioEngine *audioEngine,
                                                         TrackModel *trackModel,
                                                         PlaybackController *playbackController,
                                                         QObject *parent)
    : QObject(parent)
    , m_audioEngine(audioEngine)
    , m_trackModel(trackModel)
    , m_playbackController(playbackController)
{
    connectSignals();
}

WindowsMediaControlsService::~WindowsMediaControlsService()
{
#ifdef Q_OS_WIN
    releaseInterop();
#endif
}

void WindowsMediaControlsService::setMainWindow(QWindow *window)
{
    if (m_mainWindow == window) {
        return;
    }

#ifdef Q_OS_WIN
    releaseInterop();
#endif
    m_mainWindow = window;
    if (window) {
        connect(window, &QObject::destroyed, this, [this, window]() {
            if (m_mainWindow == window) {
                setMainWindow(nullptr);
            }
        });
    }
    updateFoundationState();
    emit mainWindowChanged(window);
}

WindowsMediaControlsService::PlaybackSnapshot WindowsMediaControlsService::playbackSnapshot() const
{
    PlaybackSnapshot snapshot;
    if (!m_audioEngine) {
        return snapshot;
    }

    snapshot.state = m_audioEngine->state();
    snapshot.positionMs = m_audioEngine->position();
    snapshot.durationMs = m_audioEngine->duration();
    return snapshot;
}

WindowsMediaControlsService::MetadataSnapshot WindowsMediaControlsService::metadataSnapshot() const
{
    MetadataSnapshot snapshot;
    QString audioFilePath;
    const bool audioEngineMetadataMatchesCurrentFile =
        !m_lastAudioEngineMetadataFilePath.isEmpty()
        && m_lastAudioEngineMetadataFilePath == (m_audioEngine ? m_audioEngine->currentFile() : QString());

    if (m_audioEngine) {
        audioFilePath = m_audioEngine->currentFile();
        snapshot.filePath = audioFilePath;
        if (audioEngineMetadataMatchesCurrentFile) {
            snapshot.title = m_audioEngine->title();
            snapshot.artist = m_audioEngine->artist();
            snapshot.album = m_audioEngine->album();
        }
    }

    if (m_trackModel) {
        const int currentIndex = m_trackModel->currentIndex();
        const QString selectedPath = currentIndex >= 0
            ? m_trackModel->getFilePath(currentIndex)
            : QString();
        const bool selectionMatchesLoadedTrack = selectedPath == audioFilePath;
        const bool canUseTrackModelFallback = audioFilePath.isEmpty() || selectionMatchesLoadedTrack;

        if (audioFilePath.isEmpty() && !selectedPath.isEmpty()) {
            snapshot.filePath = selectedPath;
        }

        if (canUseTrackModelFallback) {
            if (snapshot.title.isEmpty()) {
                snapshot.title = m_trackModel->currentTitle();
            }
            if (snapshot.artist.isEmpty()) {
                snapshot.artist = m_trackModel->currentArtist();
            }
            if (snapshot.album.isEmpty()) {
                snapshot.album = m_trackModel->currentAlbum();
            }
            snapshot.artworkUrl = m_trackModel->currentAlbumArt();
        }
    }

    if (snapshot.artworkUrl.isEmpty()
        && snapshot.filePath == m_lastPublishedMetadata.filePath) {
        snapshot.artworkUrl = m_lastPublishedMetadata.artworkUrl;
    }

    return snapshot;
}

WindowsMediaControlsService::CapabilitiesSnapshot WindowsMediaControlsService::capabilitiesSnapshot() const
{
    CapabilitiesSnapshot snapshot;

    if (m_audioEngine) {
        const bool hasLoadedTrack = !m_audioEngine->currentFile().isEmpty();
        snapshot.canPlay = hasLoadedTrack || (m_trackModel && m_trackModel->rowCount() > 0);
        snapshot.canPause = m_audioEngine->state() == AudioEngine::PlayingState;
        snapshot.canSeek = m_audioEngine->duration() > 0;
    }

    if (m_playbackController) {
        snapshot.canGoNext = m_playbackController->canGoNext();
        snapshot.canGoPrevious = m_playbackController->canGoPrevious();
    }

    return snapshot;
}

void WindowsMediaControlsService::connectSignals()
{
    if (m_audioEngine) {
        connect(m_audioEngine, &AudioEngine::stateChanged,
                this, [this](AudioEngine::PlaybackState) { updateFoundationState(); });
        connect(m_audioEngine, &AudioEngine::currentFileChanged,
                this, [this](const QString &filePath) {
            if (filePath.isEmpty()) {
                m_lastAudioEngineMetadataFilePath.clear();
            }
            updateFoundationState();
        });
        connect(m_audioEngine, &AudioEngine::durationChanged,
                this, [this](qint64) { updateFoundationState(); });
        connect(m_audioEngine, &AudioEngine::positionChanged,
                this, [this](qint64) { updateTimelineState(); });
        connect(m_audioEngine, &AudioEngine::metadataChanged,
                this, [this]() {
            if (m_audioEngine) {
                m_lastAudioEngineMetadataFilePath = m_audioEngine->currentFile();
            }
            updateFoundationState();
        });
    }

    if (m_playbackController) {
        connect(m_playbackController, &PlaybackController::navigationStateChanged,
                this, &WindowsMediaControlsService::updateFoundationState);
    }

    if (m_trackModel) {
        connect(m_trackModel, &TrackModel::countChanged,
                this, &WindowsMediaControlsService::updateFoundationState);
        connect(m_trackModel, &TrackModel::currentTrackChanged,
                this, &WindowsMediaControlsService::updateFoundationState);
    }
}

void WindowsMediaControlsService::updateFoundationState()
{
#ifdef Q_OS_WIN
    const bool wasAvailable = m_available;

    if (!ensureInteropInitialized()) {
        if (wasAvailable != m_available) {
            emit availabilityChanged(m_available);
        }
        return;
    }

    if (!registerButtonHandler()) {
        m_available = false;
        if (wasAvailable != m_available) {
            emit availabilityChanged(m_available);
        }
        return;
    }

    const PlaybackSnapshot playback = playbackSnapshot();
    const MetadataSnapshot metadata = metadataSnapshot();
    const CapabilitiesSnapshot capabilities = capabilitiesSnapshot();
    const bool hasTrackContext = hasLoadedPlaybackContext(metadata);
    const auto status = mapPlaybackStatus(playback, metadata);
    const bool capabilitiesChanged = !m_foundationStatePublished
        || !capabilitiesSnapshotsEqual(capabilities, m_lastPublishedCapabilities)
        || static_cast<int>(status) != m_lastPublishedPlaybackStatus;
    const bool metadataChanged = !m_foundationStatePublished
        || !metadataSnapshotsEqual(metadata, m_lastPublishedMetadata);

    if (!hasTrackContext) {
        smtcDiag(QStringLiteral("session deactivated reason=no-track-context"));
        deactivateSessionState();
        m_available = true;
        if (wasAvailable != m_available) {
            emit availabilityChanged(m_available);
        }
        return;
    }

    if (capabilitiesChanged) {
        m_transportControls->put_IsEnabled(TRUE);
        m_transportControls->put_IsPlayEnabled(capabilities.canPlay ? TRUE : FALSE);
        m_transportControls->put_IsPauseEnabled(capabilities.canPause ? TRUE : FALSE);
        m_transportControls->put_IsStopEnabled(TRUE);
        m_transportControls->put_IsNextEnabled(capabilities.canGoNext ? TRUE : FALSE);
        m_transportControls->put_IsPreviousEnabled(capabilities.canGoPrevious ? TRUE : FALSE);
        m_transportControls->put_PlaybackStatus(status);
        smtcDiag(QStringLiteral("session publish status=%1 canPlay=%2 canPause=%3 canNext=%4 canPrevious=%5 canSeek=%6")
                     .arg(playbackStatusString(status))
                     .arg(capabilities.canPlay ? 1 : 0)
                     .arg(capabilities.canPause ? 1 : 0)
                     .arg(capabilities.canGoNext ? 1 : 0)
                     .arg(capabilities.canGoPrevious ? 1 : 0)
                     .arg(capabilities.canSeek ? 1 : 0));
    }

    if (metadataChanged) {
        publishDisplayMetadata(m_transportControls, metadata);
    }

    updateTimelineState();
    m_lastPublishedMetadata = metadata;
    m_lastPublishedCapabilities = capabilities;
    m_lastPublishedPlaybackStatus = static_cast<int>(status);
    m_foundationStatePublished = true;
    m_available = true;

    if (wasAvailable != m_available) {
        emit availabilityChanged(m_available);
    }
#endif
}

void WindowsMediaControlsService::updateTimelineState()
{
#ifdef Q_OS_WIN
    if (!m_transportControls || !m_transportControls2) {
        return;
    }

    publishTimelineState(m_transportControls2, playbackSnapshot(), metadataSnapshot());
#endif
}

void WindowsMediaControlsService::deactivateSessionState()
{
#ifdef Q_OS_WIN
    if (!m_transportControls) {
        return;
    }

    smtcDiag(QStringLiteral("session deactivated reason=explicit-reset"));
    m_transportControls->put_IsEnabled(FALSE);
    m_transportControls->put_IsPlayEnabled(FALSE);
    m_transportControls->put_IsPauseEnabled(FALSE);
    m_transportControls->put_IsStopEnabled(FALSE);
    m_transportControls->put_IsNextEnabled(FALSE);
    m_transportControls->put_IsPreviousEnabled(FALSE);
    m_transportControls->put_PlaybackStatus(ABI::Windows::Media::MediaPlaybackStatus_Closed);
    publishDisplayMetadata(m_transportControls, MetadataSnapshot {});
    publishTimelineState(m_transportControls2, PlaybackSnapshot {}, MetadataSnapshot {});
    m_lastPublishedMetadata = MetadataSnapshot {};
    m_lastPublishedCapabilities = CapabilitiesSnapshot {};
    m_lastPublishedPlaybackStatus = static_cast<int>(ABI::Windows::Media::MediaPlaybackStatus_Closed);
    m_foundationStatePublished = false;
#endif
}

#ifdef Q_OS_WIN
bool WindowsMediaControlsService::ensureInteropInitialized()
{
    if (m_transportControls) {
        return true;
    }

    if (!m_mainWindow) {
        m_available = false;
        return false;
    }

    const HWND hwnd = reinterpret_cast<HWND>(m_mainWindow->winId());
    if (!hwnd) {
        m_available = false;
        return false;
    }

    applyWindowShellIdentity(hwnd);

    const HRESULT initHr = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(initHr) && initHr != S_FALSE && initHr != RPC_E_CHANGED_MODE) {
        qWarning() << "Failed to initialize Windows Runtime for media controls:" << Qt::hex << initHr;
        m_available = false;
        return false;
    }

    HSTRING className = nullptr;
    const wchar_t runtimeClassName[] = L"Windows.Media.SystemMediaTransportControls";
    HRESULT hr = WindowsCreateString(runtimeClassName,
                                     static_cast<UINT32>(std::size(runtimeClassName) - 1),
                                     &className);
    if (FAILED(hr)) {
        qWarning() << "Failed to create SMTC runtime class string:" << Qt::hex << hr;
        m_available = false;
        return false;
    }

    hr = RoGetActivationFactory(className,
                                IID_ISystemMediaTransportControlsInterop,
                                reinterpret_cast<void **>(&m_transportControlsInterop));
    WindowsDeleteString(className);
    if (FAILED(hr) || !m_transportControlsInterop) {
        qWarning() << "Failed to get SMTC interop activation factory:" << Qt::hex << hr;
        releaseInterop();
        m_available = false;
        return false;
    }

    hr = m_transportControlsInterop->GetForWindow(
        hwnd,
        IID___x_ABI_CWindows_CMedia_CISystemMediaTransportControls,
        reinterpret_cast<void **>(&m_transportControls));
    if (FAILED(hr) || !m_transportControls) {
        qWarning() << "Failed to get SMTC instance for window:" << Qt::hex << hr;
        releaseInterop();
        m_available = false;
        return false;
    }

    m_transportControls->QueryInterface(IID___x_ABI_CWindows_CMedia_CISystemMediaTransportControls2,
                                        reinterpret_cast<void **>(&m_transportControls2));
    smtcDiag(QStringLiteral("init success hwnd=0x%1 timelineInterop=%2")
                 .arg(QString::number(static_cast<qulonglong>(reinterpret_cast<quintptr>(hwnd)), 16))
                 .arg(m_transportControls2 ? QStringLiteral("yes") : QStringLiteral("no")));

    return true;
}

bool WindowsMediaControlsService::registerButtonHandler()
{
    if (!m_transportControls) {
        return false;
    }

    if (m_buttonPressedHandlerRegistered) {
        return true;
    }

    auto *handler = new WindowsMediaButtonPressedHandler(this);
    EventRegistrationToken token {};
    const HRESULT hr = m_transportControls->add_ButtonPressed(handler, &token);
    handler->Release();
    if (FAILED(hr)) {
        qWarning() << "Failed to register SMTC button handler:" << Qt::hex << hr;
        return false;
    }

    m_buttonPressedTokenValue = token.value;
    m_buttonPressedHandlerRegistered = true;
    smtcDiag(QStringLiteral("command handler registered token=%1").arg(m_buttonPressedTokenValue));
    return true;
}

void WindowsMediaControlsService::unregisterButtonHandler()
{
    if (!m_transportControls || !m_buttonPressedHandlerRegistered) {
        m_buttonPressedTokenValue = 0;
        m_buttonPressedHandlerRegistered = false;
        return;
    }

    EventRegistrationToken token {};
    token.value = m_buttonPressedTokenValue;
    const HRESULT hr = m_transportControls->remove_ButtonPressed(token);
    if (FAILED(hr)) {
        qWarning() << "Failed to unregister SMTC button handler:" << Qt::hex << hr;
    }

    m_buttonPressedTokenValue = 0;
    m_buttonPressedHandlerRegistered = false;
}

void WindowsMediaControlsService::dispatchButtonPressed(int buttonValue)
{
    const auto button = static_cast<ABI::Windows::Media::SystemMediaTransportControlsButton>(buttonValue);
    smtcDiag(QStringLiteral("command received button=%1").arg(buttonName(button)));
    switch (button) {
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Play:
        if (m_audioEngine) {
            m_audioEngine->play();
        }
        break;
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Pause:
        if (m_audioEngine) {
            m_audioEngine->pause();
        }
        break;
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Next:
        if (m_playbackController) {
            m_playbackController->nextTrack();
        }
        break;
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Previous:
        if (m_playbackController) {
            m_playbackController->previousTrack();
        }
        break;
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Stop:
        if (m_audioEngine) {
            m_audioEngine->stop();
        }
        break;
    default:
        break;
    }
}

void WindowsMediaControlsService::releaseInterop()
{
    deactivateSessionState();
    unregisterButtonHandler();

    if (m_transportControls2) {
        m_transportControls2->Release();
        m_transportControls2 = nullptr;
    }

    if (m_transportControls) {
        m_transportControls->Release();
        m_transportControls = nullptr;
    }

    if (m_transportControlsInterop) {
        m_transportControlsInterop->Release();
        m_transportControlsInterop = nullptr;
    }

    m_available = false;
}
#endif
