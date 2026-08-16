#pragma once

#include <QObject>
#include <QString>
#include <algorithm>
#include <cmath>

class ThemeManager;

class UiMetrics : public QObject
{
    Q_OBJECT

    // Font metrics scale & font properties
    Q_PROPERTY(double fontScale READ fontScale NOTIFY metricsChanged)
    Q_PROPERTY(double basePointSize READ basePointSize NOTIFY metricsChanged)
    Q_PROPERTY(double pointSize READ pointSize NOTIFY metricsChanged)
    Q_PROPERTY(double baseLineSpacing READ baseLineSpacing NOTIFY metricsChanged)
    Q_PROPERTY(double lineSpacing READ lineSpacing NOTIFY metricsChanged)
    Q_PROPERTY(double playlistFontScale READ playlistFontScale NOTIFY metricsChanged)
    Q_PROPERTY(double playlistLineSpacing READ playlistLineSpacing NOTIFY metricsChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY metricsChanged)
    Q_PROPERTY(QString monoFontFamily READ monoFontFamily CONSTANT)
    Q_PROPERTY(QString playlistFontFamily READ playlistFontFamily NOTIFY metricsChanged)

    // Semantic typography point sizes (Section 7.1)
    Q_PROPERTY(double microPointSize READ microPointSize NOTIFY metricsChanged)
    Q_PROPERTY(double captionPointSize READ captionPointSize NOTIFY metricsChanged)
    Q_PROPERTY(double bodyPointSize READ bodyPointSize NOTIFY metricsChanged)
    Q_PROPERTY(double bodyStrongPointSize READ bodyStrongPointSize NOTIFY metricsChanged)
    Q_PROPERTY(double subtitlePointSize READ subtitlePointSize NOTIFY metricsChanged)
    Q_PROPERTY(double titlePointSize READ titlePointSize NOTIFY metricsChanged)
    Q_PROPERTY(double displayPointSize READ displayPointSize NOTIFY metricsChanged)

    // Semantic spacing tokens (Section 7.2)
    Q_PROPERTY(int spaceXXS READ spaceXXS NOTIFY metricsChanged)
    Q_PROPERTY(int spaceXS READ spaceXS NOTIFY metricsChanged)
    Q_PROPERTY(int spaceS READ spaceS NOTIFY metricsChanged)
    Q_PROPERTY(int spaceM READ spaceM NOTIFY metricsChanged)
    Q_PROPERTY(int spaceL READ spaceL NOTIFY metricsChanged)
    Q_PROPERTY(int spaceXL READ spaceXL NOTIFY metricsChanged)
    Q_PROPERTY(int spaceXXL READ spaceXXL NOTIFY metricsChanged)

    // Interactive targets and control heights (Section 7.2)
    Q_PROPERTY(int minInteractiveTargetSize READ minInteractiveTargetSize NOTIFY metricsChanged)
    Q_PROPERTY(int controlHeightCompact READ controlHeightCompact NOTIFY metricsChanged)
    Q_PROPERTY(int controlHeightNormal READ controlHeightNormal NOTIFY metricsChanged)
    Q_PROPERTY(int controlHeightProminent READ controlHeightProminent NOTIFY metricsChanged)
    Q_PROPERTY(int controlHeight READ controlHeight NOTIFY metricsChanged)

    // Icon sizes (Section 7.2 & 8.6)
    Q_PROPERTY(int iconSizeCompact READ iconSizeCompact NOTIFY metricsChanged)
    Q_PROPERTY(int iconSizeNormal READ iconSizeNormal NOTIFY metricsChanged)
    Q_PROPERTY(int iconSizeProminent READ iconSizeProminent NOTIFY metricsChanged)
    Q_PROPERTY(int iconSizeLarge READ iconSizeLarge NOTIFY metricsChanged)

    // Playlist and table heights (Section 7.2 & 8.4)
    Q_PROPERTY(int playlistRowHeight READ playlistRowHeight NOTIFY metricsChanged)
    Q_PROPERTY(int playlistCompactRowHeight READ playlistCompactRowHeight NOTIFY metricsChanged)
    Q_PROPERTY(int playlistHeaderHeight READ playlistHeaderHeight NOTIFY metricsChanged)
    Q_PROPERTY(int playlistSortIconSize READ playlistSortIconSize NOTIFY metricsChanged)

    // Application structural heights & widths (Section 7.2)
    Q_PROPERTY(int headerHeight READ headerHeight NOTIFY metricsChanged)
    Q_PROPERTY(int controlBarHeight READ controlBarHeight NOTIFY metricsChanged)
    Q_PROPERTY(int controlBarHeightNarrow READ controlBarHeightNarrow NOTIFY metricsChanged)
    Q_PROPERTY(int compactControlsRowHeight READ compactControlsRowHeight NOTIFY metricsChanged)
    Q_PROPERTY(int compactHeaderHeight READ compactHeaderHeight NOTIFY metricsChanged)
    Q_PROPERTY(int sidebarWidth READ sidebarWidth NOTIFY metricsChanged)

    // Badges, dialogs, radii, scrollbars (Section 7.2 & 9.1)
    Q_PROPERTY(int badgePaddingHorizontal READ badgePaddingHorizontal NOTIFY metricsChanged)
    Q_PROPERTY(int badgePaddingVertical READ badgePaddingVertical NOTIFY metricsChanged)
    Q_PROPERTY(int dialogPadding READ dialogPadding NOTIFY metricsChanged)
    Q_PROPERTY(int dialogPaddingCompact READ dialogPaddingCompact NOTIFY metricsChanged)
    Q_PROPERTY(int dialogPaddingNormal READ dialogPaddingNormal NOTIFY metricsChanged)
    Q_PROPERTY(int dialogSectionSpacing READ dialogSectionSpacing NOTIFY metricsChanged)
    Q_PROPERTY(int dialogHeaderHeight READ dialogHeaderHeight NOTIFY metricsChanged)
    Q_PROPERTY(int dialogFooterHeight READ dialogFooterHeight NOTIFY metricsChanged)
    Q_PROPERTY(int radiusSmall READ radiusSmall NOTIFY metricsChanged)
    Q_PROPERTY(int radiusNormal READ radiusNormal NOTIFY metricsChanged)
    Q_PROPERTY(int radiusLarge READ radiusLarge NOTIFY metricsChanged)
    Q_PROPERTY(int scrollbarThickness READ scrollbarThickness NOTIFY metricsChanged)

public:
    explicit UiMetrics(ThemeManager *themeManager = nullptr, QObject *parent = nullptr);

    void setThemeManager(ThemeManager *themeManager);

    double fontScale() const;
    double basePointSize() const;
    double pointSize() const;
    double baseLineSpacing() const;
    double lineSpacing() const;
    double playlistFontScale() const;
    double playlistLineSpacing() const;
    QString fontFamily() const;
    QString monoFontFamily() const;
    QString playlistFontFamily() const;

    double microPointSize() const;
    double captionPointSize() const;
    double bodyPointSize() const;
    double bodyStrongPointSize() const;
    double subtitlePointSize() const;
    double titlePointSize() const;
    double displayPointSize() const;

    int spaceXXS() const;
    int spaceXS() const;
    int spaceS() const;
    int spaceM() const;
    int spaceL() const;
    int spaceXL() const;
    int spaceXXL() const;

    int minInteractiveTargetSize() const;
    int controlHeightCompact() const;
    int controlHeightNormal() const;
    int controlHeightProminent() const;
    int controlHeight() const;

    int iconSizeCompact() const;
    int iconSizeNormal() const;
    int iconSizeProminent() const;
    int iconSizeLarge() const;

    int playlistRowHeight() const;
    int playlistCompactRowHeight() const;
    int playlistHeaderHeight() const;
    int playlistSortIconSize() const;

    int headerHeight() const;
    int controlBarHeight() const;
    int controlBarHeightNarrow() const;
    int compactControlsRowHeight() const;
    int compactHeaderHeight() const;
    int sidebarWidth() const;

    int badgePaddingHorizontal() const;
    int badgePaddingVertical() const;
    int dialogPadding() const;
    int dialogPaddingCompact() const;
    int dialogPaddingNormal() const;
    int dialogSectionSpacing() const;
    int dialogHeaderHeight() const;
    int dialogFooterHeight() const;
    int radiusSmall() const;
    int radiusNormal() const;
    int radiusLarge() const;
    int scrollbarThickness() const;

    Q_INVOKABLE int scale(double value) const;
    Q_INVOKABLE int scaleCeil(double value) const;
    Q_INVOKABLE int textSafeHeight(int verticalPadding = -1) const;
    Q_INVOKABLE int textSafePlaylistHeight(int verticalPadding = -1) const;
    Q_INVOKABLE int effectiveBreakpoint(int baseWidth) const;
    Q_INVOKABLE int breakpoint(int baseWidth) const;

signals:
    void metricsChanged();

private:
    ThemeManager *m_themeManager = nullptr;
};
