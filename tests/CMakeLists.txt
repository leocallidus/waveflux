qt_add_executable(tst_equalizer_preset_manager
    tst_EqualizerPresetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AppSettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackInfoFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/EqualizerPresetManager.cpp
)

target_include_directories(tst_equalizer_preset_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(tst_equalizer_preset_manager PRIVATE
    Qt6::Core
    Qt6::Test
    KF6::I18n
)

add_test(NAME tst_equalizer_preset_manager COMMAND tst_equalizer_preset_manager)

qt_add_executable(tst_cue_sheet_parser
    tst_CueSheetParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/CueSheetParser.cpp
)

target_include_directories(tst_cue_sheet_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(tst_cue_sheet_parser PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME tst_cue_sheet_parser COMMAND tst_cue_sheet_parser)

qt_add_executable(tst_xspf_playlist_parser
    tst_XspfPlaylistParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/XspfPlaylistParser.cpp
)

target_include_directories(tst_xspf_playlist_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(tst_xspf_playlist_parser PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME tst_xspf_playlist_parser COMMAND tst_xspf_playlist_parser)

qt_add_executable(tst_app_settings_manager
    tst_AppSettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AppSettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackInfoFormatter.cpp
)

target_include_directories(tst_app_settings_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(tst_app_settings_manager PRIVATE
    Qt6::Core
    Qt6::Test
    KF6::I18n
)

add_test(NAME tst_app_settings_manager COMMAND tst_app_settings_manager)

qt_add_executable(tst_update_checker
    tst_UpdateChecker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AppSettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackInfoFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/UpdateChecker.cpp
)

target_include_directories(tst_update_checker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_definitions(tst_update_checker PRIVATE
    WAVEFLUX_UPDATECHECKER_TESTING
)

target_link_libraries(tst_update_checker PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
    KF6::I18n
)

add_test(NAME tst_update_checker COMMAND tst_update_checker)

qt_add_executable(tst_shortcut_manager
    tst_ShortcutManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ShortcutManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ShortcutRegistry.cpp
)

target_include_directories(tst_shortcut_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(tst_shortcut_manager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

add_test(NAME tst_shortcut_manager COMMAND tst_shortcut_manager)

qt_add_executable(tst_track_info_formatter
    tst_TrackInfoFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackInfoFormatter.cpp
)

target_include_directories(tst_track_info_formatter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(tst_track_info_formatter PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME tst_track_info_formatter COMMAND tst_track_info_formatter)

qt_add_executable(tst_batch_audio_converter_preset_manager
    tst_BatchAudioConverterPresetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AppSettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackInfoFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/BatchAudioConverterPresetManager.cpp
)

target_include_directories(tst_batch_audio_converter_preset_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(tst_batch_audio_converter_preset_manager PRIVATE
    Qt6::Core
    Qt6::Test
    KF6::I18n
)

add_test(NAME tst_batch_audio_converter_preset_manager COMMAND tst_batch_audio_converter_preset_manager)

qt_add_executable(tst_ytdlp_import_service
    tst_YtDlpImportService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AppSettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackInfoFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/YtDlpImportService.cpp
)

target_include_directories(tst_ytdlp_import_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_definitions(tst_ytdlp_import_service PRIVATE
    WAVEFLUX_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata"
)

target_link_libraries(tst_ytdlp_import_service PRIVATE
    Qt6::Core
    Qt6::Test
    KF6::I18n
)

add_test(NAME tst_ytdlp_import_service COMMAND tst_ytdlp_import_service)

qt_add_executable(tst_ytdlp_import_dialog_smoke
    tst_YtDlpImportDialogSmoke.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AppSettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackInfoFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ThemeManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/YtDlpImportService.cpp
)

target_include_directories(tst_ytdlp_import_dialog_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_definitions(tst_ytdlp_import_dialog_smoke PRIVATE
    WAVEFLUX_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata"
)

target_link_libraries(tst_ytdlp_import_dialog_smoke PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Test
    KF6::I18n
)

add_test(NAME tst_ytdlp_import_dialog_smoke COMMAND tst_ytdlp_import_dialog_smoke)

qt_add_executable(tst_playback_backend_contract
    tst_PlaybackBackendContract.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/IPlaybackBackend.h
)

target_include_directories(tst_playback_backend_contract PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(tst_playback_backend_contract PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME tst_playback_backend_contract COMMAND tst_playback_backend_contract)

pkg_check_modules(RUBBERBAND QUIET rubberband)

qt_add_executable(tst_openmpt_playback_backend
    tst_OpenMptPlaybackBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/IPlaybackBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptPlaybackBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptPlaybackBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/TrackerPcmEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/TrackerPcmEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/PlaybackBackendRouting.cpp
)

target_include_directories(tst_openmpt_playback_backend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${OPENMPT_INCLUDE_DIRS}
)

target_compile_definitions(tst_openmpt_playback_backend PRIVATE
    WAVEFLUX_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata"
)

target_link_libraries(tst_openmpt_playback_backend PRIVATE
    Qt6::Core
    Qt6::Multimedia
    Qt6::Test
    ${OPENMPT_LIBRARIES}
)

add_test(NAME tst_openmpt_playback_backend COMMAND tst_openmpt_playback_backend)

qt_add_executable(tracker_dsp_bench
    tracker_dsp_bench.cpp
)

target_include_directories(tracker_dsp_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${OPENMPT_INCLUDE_DIRS}
    ${RUBBERBAND_INCLUDE_DIRS}
)

target_compile_definitions(tracker_dsp_bench PRIVATE
    WAVEFLUX_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata"
    WAVEFLUX_HAVE_RUBBERBAND=$<BOOL:${RUBBERBAND_FOUND}>
)

target_link_libraries(tracker_dsp_bench PRIVATE
    Qt6::Core
    ${OPENMPT_LIBRARIES}
    $<$<BOOL:${RUBBERBAND_FOUND}>:${RUBBERBAND_LIBRARIES}>
)

qt_add_executable(tst_openmpt_waveform_renderer
    tst_OpenMptWaveformRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/IPlaybackBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptPlaybackBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptPlaybackBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/TrackerPcmEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/TrackerPcmEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptWaveformRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptWaveformRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/PlaybackBackendRouting.cpp
)

target_include_directories(tst_openmpt_waveform_renderer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${OPENMPT_INCLUDE_DIRS}
)

target_compile_definitions(tst_openmpt_waveform_renderer PRIVATE
    WAVEFLUX_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata"
)

target_link_libraries(tst_openmpt_waveform_renderer PRIVATE
    Qt6::Core
    Qt6::Multimedia
    Qt6::Test
    ${OPENMPT_LIBRARIES}
)

add_test(NAME tst_openmpt_waveform_renderer COMMAND tst_openmpt_waveform_renderer)

qt_add_executable(tst_waveform_provider
    tst_WaveformProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/WaveformProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/PeaksCacheManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/IPlaybackBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptPlaybackBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/TrackerPcmEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptWaveformRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptPlaybackBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/TrackerPcmEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/PlaybackBackendRouting.cpp
)

target_include_directories(tst_waveform_provider PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${OPENMPT_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
)

target_compile_definitions(tst_waveform_provider PRIVATE
    WAVEFLUX_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata"
)

target_link_libraries(tst_waveform_provider PRIVATE
    Qt6::Concurrent
    Qt6::Core
    Qt6::Gui
    Qt6::Multimedia
    Qt6::Test
    ${OPENMPT_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_AUDIO_LIBRARIES}
)

add_test(NAME tst_waveform_provider COMMAND tst_waveform_provider)

qt_add_executable(tst_audio_converter_service
    tst_AudioConverterService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AppSettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackInfoFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AudioConverterService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/PlaybackBackendRouting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/XspfPlaylistParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/CueSheetParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/PerformanceProfiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/library/LibraryRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/library/SearchRepository.cpp
)

target_include_directories(tst_audio_converter_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${OPENMPT_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
)

target_link_libraries(tst_audio_converter_service PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Test
    KF6::I18n
    ${OPENMPT_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_AUDIO_LIBRARIES}
    ${TAGLIB_LIBRARIES}
)

add_test(NAME tst_audio_converter_service COMMAND tst_audio_converter_service)

qt_add_executable(tst_batch_audio_converter_service
    tst_BatchAudioConverterService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AppSettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackInfoFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/BatchAudioConverterService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AudioConverterService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/PlaybackBackendRouting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/XspfPlaylistParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/CueSheetParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/PerformanceProfiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/library/LibraryRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/library/SearchRepository.cpp
)

target_include_directories(tst_batch_audio_converter_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${OPENMPT_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
)

target_link_libraries(tst_batch_audio_converter_service PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Test
    KF6::I18n
    ${OPENMPT_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_AUDIO_LIBRARIES}
    ${TAGLIB_LIBRARIES}
)

add_test(NAME tst_batch_audio_converter_service COMMAND tst_batch_audio_converter_service)

qt_add_executable(tst_playback_controller_transitions
    tst_PlaybackControllerTransitions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AppSettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackInfoFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/PlaybackController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AudioEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/RemoteTrackerSourceCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/RemoteTrackerSourceCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/SessionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/IPlaybackBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptPlaybackBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptPlaybackBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/TrackerPcmEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/TrackerPcmEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/PlaybackBackendRouting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/XspfPlaylistParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/CueSheetParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/PerformanceProfiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/library/LibraryRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/library/SearchRepository.cpp
)

target_include_directories(tst_playback_controller_transitions PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
)

target_link_libraries(tst_playback_controller_transitions PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Multimedia
    Qt6::Network
    Qt6::Quick
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Test
    KF6::I18n
    ${OPENMPT_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_AUDIO_LIBRARIES}
    ${TAGLIB_LIBRARIES}
)

add_test(NAME tst_playback_controller_transitions COMMAND tst_playback_controller_transitions)

qt_add_executable(tst_playback_controller_scenarios
    tst_PlaybackControllerScenarios.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AppSettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackInfoFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/PlaybackController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AudioEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/RemoteTrackerSourceCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/RemoteTrackerSourceCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/IPlaybackBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptPlaybackBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptPlaybackBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/TrackerPcmEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/TrackerPcmEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/PlaybackBackendRouting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/XspfPlaylistParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/CueSheetParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/PerformanceProfiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/library/LibraryRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/library/SearchRepository.cpp
)

target_include_directories(tst_playback_controller_scenarios PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
)

target_link_libraries(tst_playback_controller_scenarios PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Multimedia
    Qt6::Network
    Qt6::Quick
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Test
    KF6::I18n
    ${OPENMPT_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_AUDIO_LIBRARIES}
    ${TAGLIB_LIBRARIES}
)

add_test(NAME tst_playback_controller_scenarios COMMAND tst_playback_controller_scenarios)

qt_add_executable(tst_track_model
    tst_TrackModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/XspfPlaylistParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/CueSheetParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/PerformanceProfiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/PlaybackBackendRouting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/library/LibraryRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/library/SearchRepository.cpp
)

target_include_directories(tst_track_model PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${OPENMPT_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
)

target_link_libraries(tst_track_model PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Concurrent
    Qt6::Quick
    Qt6::Sql
    Qt6::Test
    ${OPENMPT_LIBRARIES}
    ${TAGLIB_LIBRARIES}
)

add_test(NAME tst_track_model COMMAND tst_track_model)

qt_add_executable(tst_playback_backend_routing
    tst_PlaybackBackendRouting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AppSettingsManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TrackInfoFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AudioEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/RemoteTrackerSourceCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/RemoteTrackerSourceCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/IPlaybackBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptPlaybackBackend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/OpenMptPlaybackBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/TrackerPcmEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/TrackerPcmEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/PlaybackBackendRouting.cpp
)

target_include_directories(tst_playback_backend_routing PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${OPENMPT_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
)

target_compile_definitions(tst_playback_backend_routing PRIVATE
    WAVEFLUX_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata"
)

target_link_libraries(tst_playback_backend_routing PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Multimedia
    Qt6::Network
    Qt6::Quick
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Test
    KF6::I18n
    ${OPENMPT_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_AUDIO_LIBRARIES}
    ${TAGLIB_LIBRARIES}
)

add_test(NAME tst_playback_backend_routing COMMAND tst_playback_backend_routing)

qt_add_executable(tst_remote_tracker_source_cache
    tst_RemoteTrackerSourceCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/RemoteTrackerSourceCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/RemoteTrackerSourceCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/playback/PlaybackBackendRouting.cpp
)

target_include_directories(tst_remote_tracker_source_cache PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(tst_remote_tracker_source_cache PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
)

add_test(NAME tst_remote_tracker_source_cache COMMAND tst_remote_tracker_source_cache)

find_program(WAVEFLUX_QMLLINT_EXECUTABLE qmllint)
if(WAVEFLUX_QMLLINT_EXECUTABLE)
    add_test(
        NAME tst_audio_converter_qml_smoke
        COMMAND ${WAVEFLUX_QMLLINT_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/../qml/AudioConverterDialog.qml
            ${CMAKE_CURRENT_SOURCE_DIR}/../qml/YtDlpImportDialog.qml
            ${CMAKE_CURRENT_SOURCE_DIR}/../qml/Main.qml
    )
endif()
