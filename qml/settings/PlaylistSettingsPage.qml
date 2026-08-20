import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

ColumnLayout {
    id: root

    property string searchQuery: ""
    property string targetSettingId: ""

    spacing: 12
    Layout.fillWidth: true

    function tr(key) {
        const _rev = (typeof appSettings !== "undefined" && appSettings) ? appSettings.translationRevision : 0
        return (typeof appSettings !== "undefined" && appSettings) ? appSettings.translate(key) : String(key || "")
    }

    // Group: Layout & Sidebars
    SettingsGroup {
        groupId: "layoutSidebars"
        title: root.tr("settings.groupLayoutSidebars")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "sidebarVisible"
            title: root.tr("settings.sidebarVisible")
            description: root.tr("settings.sidebarDescription")
            checked: appSettings.sidebarVisible
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.sidebarVisible = val
            }
        }

        SettingSwitchRow {
            settingId: "sidebarPlaylistsSectionVisible"
            title: root.tr("settings.sidebarPlaylistsSectionTitle")
            description: root.tr("settings.sidebarPlaylistsSectionDescription")
            checked: appSettings.sidebarPlaylistsSectionVisible
            indent: true
            dependencyReason: !appSettings.sidebarVisible
                              ? root.tr("settings.dependencyDisabledBecause").arg(root.tr("settings.sidebarVisible"))
                              : ""
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.sidebarPlaylistsSectionVisible = val
            }
        }

        SettingSwitchRow {
            settingId: "sidebarCollectionsSectionVisible"
            title: root.tr("settings.sidebarCollectionsSectionTitle")
            description: root.tr("settings.sidebarCollectionsSectionDescription")
            checked: appSettings.sidebarCollectionsSectionVisible
            indent: true
            dependencyReason: !appSettings.sidebarVisible
                              ? root.tr("settings.dependencyDisabledBecause").arg(root.tr("settings.sidebarVisible"))
                              : ""
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.sidebarCollectionsSectionVisible = val
            }
        }
    }

    // Group: Columns & Rows
    SettingsGroup {
        groupId: "columnsRows"
        title: root.tr("settings.groupColumnsRows")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "compactPlaylistTrackNumberVisible"
            title: root.tr("settings.compactPlaylistTrackNumberVisible")
            description: root.tr("settings.compactPlaylistTrackNumberVisibleDescription")
            checked: appSettings.compactPlaylistTrackNumberVisible
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.compactPlaylistTrackNumberVisible = val
            }
        }

        SettingSwitchRow {
            settingId: "showPlaylistChapterBadge"
            title: root.tr("settings.showPlaylistChapterBadge")
            description: root.tr("settings.showPlaylistChapterBadgeDescription")
            checked: appSettings.showPlaylistChapterBadge
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.showPlaylistChapterBadge = val
            }
        }

        SettingSwitchRow {
            settingId: "playlistScrollBarVisible"
            title: root.tr("settings.playlistScrollBarVisible")
            description: root.tr("settings.playlistScrollBarVisibleDescription")
            checked: appSettings.playlistScrollBarVisible
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.playlistScrollBarVisible = val
            }
        }

        SettingActionRow {
            settingId: "configurePlaylistColumns"
            title: root.tr("settings.configurePlaylistColumns")
            description: root.tr("settings.configurePlaylistColumnsDescription")
            buttonText: root.tr("settings.configurePlaylistColumns")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onClicked: {
                columnsDialog.open()
            }
        }
    }

    // Group: Search & Navigation
    SettingsGroup {
        groupId: "searchNavigation"
        title: root.tr("settings.groupSearchNavigation")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "playSearchResultsInOrder"
            title: root.tr("settings.playSearchResultsInOrder")
            description: root.tr("settings.playSearchResultsInOrderDescription")
            checked: appSettings.playSearchResultsInOrder
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.playSearchResultsInOrder = val
            }
        }

        SettingSwitchRow {
            settingId: "automaticPlaylistSearch"
            title: root.tr("settings.automaticPlaylistSearch")
            description: root.tr("settings.automaticPlaylistSearchDescription")
            checked: appSettings.automaticPlaylistSearch
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.automaticPlaylistSearch = val
            }
        }
    }

    // Group: Adding and Opening Tracks
    SettingsGroup {
        groupId: "addingOpening"
        title: root.tr("settings.groupAddingOpening")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "playExternalOpenWithoutPlaylist"
            title: root.tr("settings.playExternalOpenWithoutPlaylist")
            description: root.tr("settings.playExternalOpenWithoutPlaylistDescription")
            checked: appSettings.playExternalOpenWithoutPlaylist
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.playExternalOpenWithoutPlaylist = val
            }
        }

        SettingSwitchRow {
            settingId: "autoAddTracksFromPlaylistFolder"
            title: root.tr("settings.autoAddTracksFromPlaylistFolder")
            description: root.tr("settings.autoAddTracksFromPlaylistFolderDescription")
            checked: appSettings.autoAddTracksFromPlaylistFolder
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.autoAddTracksFromPlaylistFolder = val
            }
        }
    }

    // Group: File Operations
    SettingsGroup {
        groupId: "fileOperations"
        title: root.tr("settings.groupFileOperations")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "confirmMoveTrackToTrash"
            title: root.tr("settings.confirmTrash")
            description: root.tr("settings.confirmTrashDescription")
            checked: appSettings.confirmMoveTrackToTrash
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.confirmMoveTrackToTrash = val
            }
        }
    }

    PlaylistColumnsDialog {
        id: columnsDialog
    }
}
