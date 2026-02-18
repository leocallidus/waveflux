cmake_minimum_required(VERSION 3.21)
project(waveflux VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 packages
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Quick
    Qml
    Gui
    Sql
    DBus
    Concurrent
    Widgets
    Test
    QuickControls2
)

# Find Kirigami
find_package(KF6Kirigami REQUIRED)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6I18n REQUIRED)

# Avoid hard-linking against exact KF patch-level .so filenames in generated
# link commands. This keeps existing build dirs resilient across distro updates
# (e.g. 6.22.x -> 6.23.x) after a lightweight reconfigure.
function(waveflux_relax_imported_soname target_name)
    if(TARGET "${target_name}")
        set(_resolved_target "${target_name}")
        get_target_property(_aliased_target "${target_name}" ALIASED_TARGET)
        if(_aliased_target)
            set(_resolved_target "${_aliased_target}")
        endif()

        if(TARGET "${_resolved_target}")
            get_target_property(_is_imported "${_resolved_target}" IMPORTED)
            if(_is_imported)
                set_property(TARGET "${_resolved_target}" PROPERTY IMPORTED_NO_SONAME TRUE)
            endif()
        endif()
    endif()
endfunction()

waveflux_relax_imported_soname(KF6::Kirigami)
waveflux_relax_imported_soname(KF6::KirigamiPlatform)
waveflux_relax_imported_soname(KF6::CoreAddons)
waveflux_relax_imported_soname(KF6::I18n)

# Find GStreamer using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_AUDIO REQUIRED gstreamer-audio-1.0)

# Find TagLib
pkg_check_modules(TAGLIB REQUIRED taglib)

# Source files
set(SOURCES
    src/main.cpp
    src/AudioEngine.cpp
    src/WaveformProvider.cpp
    src/TrackModel.cpp
    src/XspfPlaylistParser.cpp
    src/CueSheetParser.cpp
    src/WaveformItem.cpp
    src/TagEditor.cpp
    src/ThemeManager.cpp
    src/PlaybackController.cpp
    src/SessionManager.cpp
    src/PlaylistExportService.cpp
    src/PlaylistProfilesManager.cpp
    src/EqualizerPresetManager.cpp
    src/AppSettingsManager.cpp
    src/MprisService.cpp
    src/TrayManager.cpp
    src/XdgPortalFilePicker.cpp
    src/PeaksCacheManager.cpp
    src/PerformanceProfiler.cpp
    src/library/DatabaseManager.cpp
    src/library/MigrationManager.cpp
    src/library/LibraryRepository.cpp
    src/library/SearchRepository.cpp
    src/library/SmartCollectionsEngine.cpp
)

set(HEADERS
    src/AudioEngine.h
    src/WaveformProvider.h
    src/PeaksCacheManager.h
    src/TrackModel.h
    src/XspfPlaylistParser.h
    src/CueSheetParser.h
    src/WaveformItem.h
    src/TagEditor.h
    src/ThemeManager.h
    src/PlaybackController.h
    src/SessionManager.h
    src/PlaylistExportService.h
    src/PlaylistProfilesManager.h
    src/EqualizerPresetManager.h
    src/AppSettingsManager.h
    src/MprisService.h
    src/TrayManager.h
    src/XdgPortalFilePicker.h
    src/PerformanceProfiler.h
    src/library/DatabaseManager.h
    src/library/MigrationManager.h
    src/library/LibraryRepository.h
    src/library/SearchRepository.h
    src/library/SmartCollectionsEngine.h
)

# Create executable
qt_add_executable(waveflux
    ${SOURCES}
    ${HEADERS}
)

# Add QML module
file(GLOB WAVEFLUX_ICON_RESOURCES
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "resources/icons/*.svg"
)

qt_add_qml_module(waveflux
    URI WaveFlux
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/CompactSkin.qml
        qml/components/HeaderBar.qml
        qml/components/PlaylistTable.qml
        qml/components/InfoSidebar.qml
        qml/components/ControlBar.qml
        qml/components/SettingToggleRow.qml
        qml/components/SettingSliderRow.qml
        qml/components/SettingComboRow.qml
        qml/components/SettingHintText.qml
        qml/WaveformView.qml
        qml/PlayerControls.qml
        qml/PlaylistView.qml
        qml/TagEditorDialog.qml
        qml/BulkTagEditorDialog.qml
        qml/components/AboutDialog.qml
        qml/components/KeyboardShortcutsDialog.qml
        qml/SettingsDialog.qml
        qml/EqualizerDialog.qml
        qml/SmartCollectionDialog.qml
        qml/components/CollectionsSidebar.qml
    RESOURCES
        qml/IconResolver.js
        ${WAVEFLUX_ICON_RESOURCES}
)

# Include directories
target_include_directories(waveflux PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(waveflux PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Qml
    Qt6::Gui
    Qt6::Sql
    Qt6::DBus
    Qt6::Concurrent
    Qt6::Widgets
    Qt6::QuickControls2
    KF6::Kirigami
    KF6::CoreAddons
    KF6::I18n
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_AUDIO_LIBRARIES}
    ${TAGLIB_LIBRARIES}
)

# Compiler definitions
target_compile_definitions(waveflux PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Install rules
install(TARGETS waveflux
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
