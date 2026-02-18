qt_add_executable(tst_equalizer_preset_manager
    tst_EqualizerPresetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/EqualizerPresetManager.cpp
)

target_include_directories(tst_equalizer_preset_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(tst_equalizer_preset_manager PRIVATE
    Qt6::Core
    Qt6::Test
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

qt_add_executable(tst_playback_controller_transitions
    tst_PlaybackControllerTransitions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/PlaybackController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AudioEngine.cpp
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
    Qt6::Quick
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Test
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_AUDIO_LIBRARIES}
    ${TAGLIB_LIBRARIES}
)

add_test(NAME tst_playback_controller_transitions COMMAND tst_playback_controller_transitions)

qt_add_executable(tst_playback_controller_scenarios
    tst_PlaybackControllerScenarios.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/PlaybackController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/AudioEngine.cpp
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
    Qt6::Quick
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Test
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_AUDIO_LIBRARIES}
    ${TAGLIB_LIBRARIES}
)

add_test(NAME tst_playback_controller_scenarios COMMAND tst_playback_controller_scenarios)
