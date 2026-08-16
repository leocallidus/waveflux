#include "UiMetrics.h"
#include "ThemeManager.h"

UiMetrics::UiMetrics(ThemeManager *themeManager, QObject *parent)
    : QObject(parent)
{
    setThemeManager(themeManager);
}

void UiMetrics::setThemeManager(ThemeManager *themeManager)
{
    if (m_themeManager == themeManager) {
        return;
    }
    if (m_themeManager) {
        disconnect(m_themeManager, nullptr, this, nullptr);
    }
    m_themeManager = themeManager;
    if (m_themeManager) {
        connect(m_themeManager, &ThemeManager::themeChanged, this, &UiMetrics::metricsChanged);
        connect(m_themeManager, &ThemeManager::fontMetricsScaleChanged, this, &UiMetrics::metricsChanged);
        connect(m_themeManager, &ThemeManager::effectiveFontPointSizeChanged, this, &UiMetrics::metricsChanged);
        connect(m_themeManager, &ThemeManager::effectiveFontLineSpacingChanged, this, &UiMetrics::metricsChanged);
        connect(m_themeManager, &ThemeManager::playlistFontMetricsScaleChanged, this, &UiMetrics::metricsChanged);
        connect(m_themeManager, &ThemeManager::playlistFontLineSpacingChanged, this, &UiMetrics::metricsChanged);
        connect(m_themeManager, &ThemeManager::fontFamilyChanged, this, &UiMetrics::metricsChanged);
        connect(m_themeManager, &ThemeManager::playlistFontFamilyChanged, this, &UiMetrics::metricsChanged);
    }
    emit metricsChanged();
}

double UiMetrics::fontScale() const
{
    return m_themeManager ? m_themeManager->fontMetricsScale() : 1.0;
}

double UiMetrics::basePointSize() const
{
    return m_themeManager ? m_themeManager->baseFontPointSize() : 10.0;
}

double UiMetrics::pointSize() const
{
    return m_themeManager ? m_themeManager->effectiveFontPointSize() : 10.0;
}

double UiMetrics::baseLineSpacing() const
{
    return m_themeManager ? m_themeManager->baseFontLineSpacing() : 14.0;
}

double UiMetrics::lineSpacing() const
{
    return m_themeManager ? m_themeManager->effectiveFontLineSpacing() : 14.0;
}

double UiMetrics::playlistFontScale() const
{
    return m_themeManager ? m_themeManager->playlistFontMetricsScale() : fontScale();
}

double UiMetrics::playlistLineSpacing() const
{
    return m_themeManager ? m_themeManager->playlistFontLineSpacing() : lineSpacing();
}

QString UiMetrics::fontFamily() const
{
    return m_themeManager ? m_themeManager->fontFamily() : QString();
}

QString UiMetrics::monoFontFamily() const
{
    return m_themeManager ? m_themeManager->monoFontFamily() : QStringLiteral("monospace");
}

QString UiMetrics::playlistFontFamily() const
{
    return m_themeManager ? m_themeManager->playlistFontFamily() : fontFamily();
}

double UiMetrics::microPointSize() const
{
    return std::max(6.0, std::round(pointSize() * 0.7));
}

double UiMetrics::captionPointSize() const
{
    return std::max(7.0, std::round(pointSize() * 0.85));
}

double UiMetrics::bodyPointSize() const
{
    return pointSize();
}

double UiMetrics::bodyStrongPointSize() const
{
    return pointSize();
}

double UiMetrics::subtitlePointSize() const
{
    return std::round(pointSize() * 1.15);
}

double UiMetrics::titlePointSize() const
{
    return std::round(pointSize() * 1.35);
}

double UiMetrics::displayPointSize() const
{
    return std::round(pointSize() * 1.75);
}

int UiMetrics::spaceXXS() const
{
    return std::max(2, static_cast<int>(std::round(2.0 * fontScale())));
}

int UiMetrics::spaceXS() const
{
    return std::max(4, static_cast<int>(std::round(4.0 * fontScale())));
}

int UiMetrics::spaceS() const
{
    return std::max(6, static_cast<int>(std::round(6.0 * fontScale())));
}

int UiMetrics::spaceM() const
{
    return std::max(8, static_cast<int>(std::round(8.0 * fontScale())));
}

int UiMetrics::spaceL() const
{
    return std::max(12, static_cast<int>(std::round(12.0 * fontScale())));
}

int UiMetrics::spaceXL() const
{
    return std::max(16, static_cast<int>(std::round(16.0 * fontScale())));
}

int UiMetrics::spaceXXL() const
{
    return std::max(24, static_cast<int>(std::round(24.0 * fontScale())));
}

int UiMetrics::minInteractiveTargetSize() const
{
    return std::max(28, static_cast<int>(std::ceil(28.0 * fontScale())));
}

int UiMetrics::controlHeightCompact() const
{
    return std::max(24, static_cast<int>(std::ceil(lineSpacing() + spaceXS() * 2)));
}

int UiMetrics::controlHeightNormal() const
{
    return std::max(32, static_cast<int>(std::ceil(lineSpacing() + spaceS() * 2)));
}

int UiMetrics::controlHeightProminent() const
{
    return std::max(40, static_cast<int>(std::ceil(lineSpacing() + spaceM() * 2)));
}

int UiMetrics::controlHeight() const
{
    return controlHeightNormal();
}

int UiMetrics::iconSizeCompact() const
{
    return std::max(14, static_cast<int>(std::round(14.0 * fontScale())));
}

int UiMetrics::iconSizeNormal() const
{
    return std::max(16, static_cast<int>(std::round(16.0 * fontScale())));
}

int UiMetrics::iconSizeProminent() const
{
    return std::max(22, static_cast<int>(std::round(22.0 * fontScale())));
}

int UiMetrics::iconSizeLarge() const
{
    return std::max(28, static_cast<int>(std::round(28.0 * fontScale())));
}

int UiMetrics::playlistRowHeight() const
{
    return std::max(28, static_cast<int>(std::ceil(playlistLineSpacing() + spaceXS() * 2)));
}

int UiMetrics::playlistCompactRowHeight() const
{
    return std::max(24, static_cast<int>(std::ceil(playlistLineSpacing() + spaceXXS() * 2)));
}

int UiMetrics::playlistHeaderHeight() const
{
    return std::max(28, static_cast<int>(std::ceil(lineSpacing() + spaceXS() * 2)));
}

int UiMetrics::playlistSortIconSize() const
{
    return std::max(12, static_cast<int>(std::round(12.0 * fontScale())));
}

int UiMetrics::headerHeight() const
{
    return std::max(40, static_cast<int>(std::ceil(40.0 * fontScale())));
}

int UiMetrics::controlBarHeight() const
{
    return std::max(88, static_cast<int>(std::ceil(88.0 * fontScale())));
}

int UiMetrics::controlBarHeightNarrow() const
{
    return std::max(118, static_cast<int>(std::ceil(118.0 * fontScale())));
}

int UiMetrics::compactControlsRowHeight() const
{
    return std::max(48, static_cast<int>(std::ceil(48.0 * fontScale())));
}

int UiMetrics::compactHeaderHeight() const
{
    return std::max(36, static_cast<int>(std::ceil(36.0 * fontScale())));
}

int UiMetrics::sidebarWidth() const
{
    return std::max(240, static_cast<int>(std::round(256.0 * fontScale())));
}

int UiMetrics::badgePaddingHorizontal() const
{
    return std::max(6, static_cast<int>(std::round(6.0 * fontScale())));
}

int UiMetrics::badgePaddingVertical() const
{
    return std::max(2, static_cast<int>(std::round(2.0 * fontScale())));
}

int UiMetrics::dialogPadding() const
{
    return std::max(16, static_cast<int>(std::round(16.0 * fontScale())));
}

int UiMetrics::dialogPaddingCompact() const
{
    return std::max(10, static_cast<int>(std::round(10.0 * fontScale())));
}

int UiMetrics::dialogPaddingNormal() const
{
    return std::max(16, static_cast<int>(std::round(16.0 * fontScale())));
}

int UiMetrics::dialogSectionSpacing() const
{
    return std::max(12, static_cast<int>(std::round(12.0 * fontScale())));
}

int UiMetrics::dialogHeaderHeight() const
{
    return std::max(44, static_cast<int>(std::ceil(lineSpacing() + spaceL() * 2)));
}

int UiMetrics::dialogFooterHeight() const
{
    return std::max(52, static_cast<int>(std::ceil(controlHeightNormal() + spaceM() * 2)));
}

int UiMetrics::radiusSmall() const
{
    return std::max(3, static_cast<int>(std::round(4.0 * fontScale())));
}

int UiMetrics::radiusNormal() const
{
    return std::max(4, static_cast<int>(std::round(6.0 * fontScale())));
}

int UiMetrics::radiusLarge() const
{
    return std::max(8, static_cast<int>(std::round(8.0 * fontScale())));
}

int UiMetrics::scrollbarThickness() const
{
    return std::max(6, static_cast<int>(std::round(6.0 * fontScale())));
}

int UiMetrics::scale(double value) const
{
    return static_cast<int>(std::round(value * fontScale()));
}

int UiMetrics::scaleCeil(double value) const
{
    return static_cast<int>(std::ceil(value * fontScale()));
}

int UiMetrics::textSafeHeight(int verticalPadding) const
{
    const int pad = verticalPadding >= 0 ? verticalPadding : spaceS();
    return static_cast<int>(std::ceil(lineSpacing() + pad * 2));
}

int UiMetrics::textSafePlaylistHeight(int verticalPadding) const
{
    const int pad = verticalPadding >= 0 ? verticalPadding : spaceXS();
    return static_cast<int>(std::ceil(playlistLineSpacing() + pad * 2));
}

int UiMetrics::effectiveBreakpoint(int baseWidth) const
{
    const double factor = std::clamp(fontScale(), 0.85, 1.6);
    return static_cast<int>(std::round(baseWidth * factor));
}

int UiMetrics::breakpoint(int baseWidth) const
{
    return effectiveBreakpoint(baseWidth);
}
