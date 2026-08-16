#include "ThemeManager.h"
#include <QStyleHints>
#include <QFontMetricsF>
#include <algorithm>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_settings("WaveFlux", "WaveFlux")
    , m_customFontFamily("Default")
    , m_customFontSize(0)
    , m_playlistFontFamily("Default")
{
    // Capture baseline system font before applying any WaveFlux custom fonts
    m_baselineSystemFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    if (m_baselineSystemFont.pointSizeF() <= 0.0) {
        if (m_baselineSystemFont.pointSize() > 0) {
            m_baselineSystemFont.setPointSizeF(m_baselineSystemFont.pointSize());
        } else {
            m_baselineSystemFont.setPointSizeF(10.0);
        }
    }
    QFontMetricsF baseFm(m_baselineSystemFont);
    m_baseFontLineSpacing = baseFm.lineSpacing();
    if (m_baseFontLineSpacing <= 0.0) {
        m_baseFontLineSpacing = baseFm.height() > 0.0 ? baseFm.height() : 14.0;
    }
    m_baseFontPointSize = m_baselineSystemFont.pointSizeF();

    // Initialize with system palette
    applySystemPalette();
    loadSettings();
    
    // Connect to system theme changes
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this]() {
        applySystemPalette();
        if (m_customFontFamily == QStringLiteral("Default") || m_customFontSize == 0) {
            QFont sysFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
            if (sysFont.pointSizeF() <= 0.0) {
                sysFont.setPointSizeF(sysFont.pointSize() > 0 ? sysFont.pointSize() : 10.0);
            }
            m_baselineSystemFont = sysFont;
            QFontMetricsF sysFm(m_baselineSystemFont);
            m_baseFontLineSpacing = sysFm.lineSpacing() > 0.0 ? sysFm.lineSpacing() : (sysFm.height() > 0.0 ? sysFm.height() : 14.0);
            m_baseFontPointSize = m_baselineSystemFont.pointSizeF();
            updateApplicationFont();
        }
        emit themeChanged();
    });
}

ThemeManager::~ThemeManager()
{
    if (!m_persistenceSuppressed) {
        saveSettings();
    }
}

void ThemeManager::setWaveformColor(const QColor &color)
{
    if (m_waveformColor != color) {
        m_waveformColor = color;
        emit waveformColorChanged();
        emit themeChanged();
    }
}

void ThemeManager::setWaveformBackgroundColor(const QColor &color)
{
    if (m_waveformBackgroundColor != color) {
        m_waveformBackgroundColor = color;
        emit waveformBackgroundColorChanged();
        emit themeChanged();
    }
}

void ThemeManager::setProgressColor(const QColor &color)
{
    if (m_progressColor != color) {
        m_progressColor = color;
        emit progressColorChanged();
        emit themeChanged();
    }
}

void ThemeManager::setPrimaryColor(const QColor &color)
{
    if (m_primaryColor != color) {
        m_primaryColor = color;
        if (m_accentColor != color) {
            m_accentColor = color;
            emit accentColorChanged();
        }
        emit primaryColorChanged();
        emit themeChanged();
    }
}

void ThemeManager::setBackgroundColor(const QColor &color)
{
    if (m_backgroundColor != color) {
        m_backgroundColor = color;
        emit backgroundColorChanged();
        emit themeChanged();
    }
}

void ThemeManager::setSurfaceColor(const QColor &color)
{
    if (m_surfaceColor != color) {
        m_surfaceColor = color;
        emit surfaceColorChanged();
        emit themeChanged();
    }
}

void ThemeManager::setBorderColor(const QColor &color)
{
    if (m_borderColor != color) {
        m_borderColor = color;
        emit borderColorChanged();
        emit themeChanged();
    }
}

void ThemeManager::setAccentColor(const QColor &color)
{
    if (m_accentColor != color) {
        m_accentColor = color;
        if (m_primaryColor != color) {
            m_primaryColor = color;
            emit primaryColorChanged();
        }
        emit accentColorChanged();
        emit themeChanged();
    }
}

void ThemeManager::setTextColor(const QColor &color)
{
    if (m_textColor != color) {
        m_textColor = color;
        emit textColorChanged();
        emit themeChanged();
    }
}

void ThemeManager::setTextSecondaryColor(const QColor &color)
{
    if (m_textSecondaryColor != color) {
        m_textSecondaryColor = color;
        emit textSecondaryColorChanged();
        emit themeChanged();
    }
}

void ThemeManager::setTextMutedColor(const QColor &color)
{
    if (m_textMutedColor != color) {
        m_textMutedColor = color;
        emit textMutedColorChanged();
        emit themeChanged();
    }
}

void ThemeManager::setDarkMode(bool dark)
{
    if (m_darkMode != dark) {
        m_darkMode = dark;
        // Dark mode toggle no longer changes colors - they come from system
        applySystemPalette();
        emit darkModeChanged();
        emit themeChanged();
    }
}

void ThemeManager::loadTheme(const QString &name)
{
    m_settings.beginGroup("Themes/" + name);
    
    // Start with system palette
    applySystemPalette();
    
    if (m_settings.contains("waveformColor")) {
        m_waveformColor = QColor(m_settings.value("waveformColor").toString());
        m_waveformBackgroundColor = QColor(
            m_settings.value("waveformBackgroundColor", m_waveformBackgroundColor.name()).toString());
        m_progressColor = QColor(m_settings.value("progressColor").toString());
        m_accentColor = QColor(m_settings.value("accentColor").toString());
        m_primaryColor = QColor(m_settings.value("primaryColor", m_accentColor.name()).toString());
    }
    
    m_settings.endGroup();
    
    emit waveformColorChanged();
    emit waveformBackgroundColorChanged();
    emit progressColorChanged();
    emit primaryColorChanged();
    emit backgroundColorChanged();
    emit surfaceColorChanged();
    emit borderColorChanged();
    emit accentColorChanged();
    emit textColorChanged();
    emit textSecondaryColorChanged();
    emit textMutedColorChanged();
    emit darkModeChanged();
    emit themeChanged();
}

void ThemeManager::saveCurrentTheme(const QString &name)
{
    m_settings.beginGroup("Themes/" + name);
    m_settings.setValue("waveformColor", m_waveformColor.name());
    m_settings.setValue("waveformBackgroundColor", m_waveformBackgroundColor.name());
    m_settings.setValue("progressColor", m_progressColor.name());
    m_settings.setValue("primaryColor", m_primaryColor.name());
    m_settings.setValue("backgroundColor", m_backgroundColor.name());
    m_settings.setValue("surfaceColor", m_surfaceColor.name());
    m_settings.setValue("borderColor", m_borderColor.name());
    m_settings.setValue("accentColor", m_accentColor.name());
    m_settings.setValue("textColor", m_textColor.name());
    m_settings.setValue("textSecondaryColor", m_textSecondaryColor.name());
    m_settings.setValue("textMutedColor", m_textMutedColor.name());
    m_settings.setValue("darkMode", m_darkMode);
    m_settings.endGroup();
    m_settings.sync();
}

QStringList ThemeManager::availableThemes() const
{
    QStringList themes;
    themes << "Default Dark" << "Default Light" << "Teal" << "Purple";
    
    // Add custom themes from settings
    const_cast<QSettings&>(m_settings).beginGroup("Themes");
    themes.append(const_cast<QSettings&>(m_settings).childGroups());
    const_cast<QSettings&>(m_settings).endGroup();
    
    return themes;
}

void ThemeManager::resetToDefault()
{
    m_settings.remove(QStringLiteral("Theme"));
    applySystemPalette();

    m_customFontFamily = QStringLiteral("Default");
    m_customFontSize = 0;
    m_playlistFontFamily = QStringLiteral("Default");

    // Refresh baseline system font in case it changed
    m_baselineSystemFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    if (m_baselineSystemFont.pointSizeF() <= 0.0) {
        if (m_baselineSystemFont.pointSize() > 0) {
            m_baselineSystemFont.setPointSizeF(m_baselineSystemFont.pointSize());
        } else {
            m_baselineSystemFont.setPointSizeF(10.0);
        }
    }
    QFontMetricsF baseFm(m_baselineSystemFont);
    m_baseFontLineSpacing = baseFm.lineSpacing() > 0.0 ? baseFm.lineSpacing() : (baseFm.height() > 0.0 ? baseFm.height() : 14.0);
    m_baseFontPointSize = m_baselineSystemFont.pointSizeF();

    emit customFontFamilyChanged();
    emit customFontSizeChanged();
    updateApplicationFont();

    emit waveformColorChanged();
    emit waveformBackgroundColorChanged();
    emit progressColorChanged();
    emit primaryColorChanged();
    emit backgroundColorChanged();
    emit surfaceColorChanged();
    emit borderColorChanged();
    emit accentColorChanged();
    emit textColorChanged();
    emit textSecondaryColorChanged();
    emit textMutedColorChanged();
    emit darkModeChanged();
    emit themeChanged();

    saveSettings();
}

void ThemeManager::loadSettings()
{
    m_settings.beginGroup("Theme");

    if (m_settings.contains("waveformColor")) {
        m_waveformColor = QColor(m_settings.value("waveformColor").toString());
    }
    if (m_settings.contains("waveformBackgroundColor")) {
        m_waveformBackgroundColor = QColor(m_settings.value("waveformBackgroundColor").toString());
    }
    if (m_settings.contains("progressColor")) {
        m_progressColor = QColor(m_settings.value("progressColor").toString());
    }
    if (m_settings.contains("primaryColor")) {
        m_primaryColor = QColor(m_settings.value("primaryColor").toString());
    }
    if (m_settings.contains("accentColor")) {
        m_accentColor = QColor(m_settings.value("accentColor").toString());
    } else {
        m_accentColor = m_primaryColor;
    }
    if (m_settings.contains("customFontFamily")) {
        m_customFontFamily = m_settings.value("customFontFamily").toString();
    } else {
        m_customFontFamily = QStringLiteral("Default");
    }
    if (m_settings.contains("customFontSize")) {
        int loadedSize = m_settings.value("customFontSize").toInt();
        if (loadedSize != 0) {
            loadedSize = std::clamp(loadedSize, 8, 24);
        }
        m_customFontSize = loadedSize;
    } else {
        m_customFontSize = 0;
    }
    if (m_settings.contains("playlistFontFamily")) {
        m_playlistFontFamily = m_settings.value("playlistFontFamily").toString();
    } else {
        m_playlistFontFamily = QStringLiteral("Default");
    }
    m_settings.endGroup();
    
    updateApplicationFont();
}

void ThemeManager::saveSettings()
{
    m_settings.beginGroup("Theme");
    // Only save user-customizable colors (waveform, waveform background, progress, accent)
    m_settings.setValue("waveformColor", m_waveformColor.name());
    m_settings.setValue("waveformBackgroundColor", m_waveformBackgroundColor.name());
    m_settings.setValue("progressColor", m_progressColor.name());
    m_settings.setValue("primaryColor", m_primaryColor.name());
    m_settings.setValue("accentColor", m_accentColor.name());
    m_settings.setValue("customFontFamily", m_customFontFamily);
    m_settings.setValue("customFontSize", m_customFontSize);
    m_settings.setValue("playlistFontFamily", m_playlistFontFamily);
    m_settings.endGroup();
    m_settings.sync();
}

void ThemeManager::applyDarkTheme()
{
    applySystemPalette();
    m_darkMode = true;
    
    emit waveformColorChanged();
    emit waveformBackgroundColorChanged();
    emit progressColorChanged();
    emit primaryColorChanged();
    emit backgroundColorChanged();
    emit surfaceColorChanged();
    emit borderColorChanged();
    emit accentColorChanged();
    emit textColorChanged();
    emit textSecondaryColorChanged();
    emit textMutedColorChanged();
}

void ThemeManager::applyLightTheme()
{
    applySystemPalette();
    m_darkMode = false;
    
    emit waveformColorChanged();
    emit waveformBackgroundColorChanged();
    emit progressColorChanged();
    emit primaryColorChanged();
    emit backgroundColorChanged();
    emit surfaceColorChanged();
    emit borderColorChanged();
    emit accentColorChanged();
    emit textColorChanged();
    emit textSecondaryColorChanged();
    emit textMutedColorChanged();
}

void ThemeManager::applySystemPalette()
{
    QPalette palette = QGuiApplication::palette();
    
    // Use system colors from palette
    m_backgroundColor = palette.color(QPalette::Window);
    m_surfaceColor = palette.color(QPalette::Base);
    m_textColor = palette.color(QPalette::WindowText);
    m_textSecondaryColor = palette.color(QPalette::PlaceholderText);
    m_textMutedColor = palette.color(QPalette::Disabled, QPalette::WindowText);
    m_borderColor = palette.color(QPalette::Mid);
    m_accentColor = palette.color(QPalette::Highlight);
    m_primaryColor = palette.color(QPalette::Highlight);
    m_progressColor = palette.color(QPalette::Highlight);
    m_waveformColor = QColor(QStringLiteral("#00786b"));
    m_waveformBackgroundColor = m_backgroundColor;
    
    // Detect dark mode from system
    m_darkMode = (palette.color(QPalette::Window).lightness() < 128);
}

QString ThemeManager::fontFamily() const
{
    if (!m_customFontFamily.isEmpty() && m_customFontFamily != QStringLiteral("Default")) {
        const QStringList families = QFontDatabase::families();
        if (families.contains(m_customFontFamily)) {
            return m_customFontFamily;
        }
    }
    return m_baselineSystemFont.family();
}

void ThemeManager::setCustomFontFamily(const QString &family)
{
    if (m_customFontFamily != family) {
        m_customFontFamily = family;
        emit customFontFamilyChanged();
        updateApplicationFont();
    }
}

void ThemeManager::setCustomFontSize(int size)
{
    int validatedSize = size;
    if (validatedSize != 0) {
        validatedSize = std::clamp(validatedSize, 8, 24);
    }
    if (m_customFontSize != validatedSize) {
        m_customFontSize = validatedSize;
        emit customFontSizeChanged();
        updateApplicationFont();
    }
}

double ThemeManager::fontSizeMultiplier() const
{
    return m_fontMetricsScale;
}

QStringList ThemeManager::availableFonts() const
{
    QStringList fonts;
    fonts << QStringLiteral("Default");
    fonts.append(QFontDatabase::families());
    return fonts;
}

void ThemeManager::updateApplicationFont()
{
    QFont font = m_baselineSystemFont;
    QString effectiveFamily = m_baselineSystemFont.family();
    if (!m_customFontFamily.isEmpty() && m_customFontFamily != QStringLiteral("Default")) {
        const QStringList families = QFontDatabase::families();
        if (families.contains(m_customFontFamily)) {
            effectiveFamily = m_customFontFamily;
        }
    }
    font.setFamily(effectiveFamily);
    
    qreal effectivePointSize = m_baseFontPointSize;
    if (m_customFontSize >= 8 && m_customFontSize <= 24) {
        effectivePointSize = static_cast<qreal>(m_customFontSize);
    }
    font.setPointSizeF(effectivePointSize);
    
    QGuiApplication::setFont(font);

    QFontMetricsF fm(font);
    qreal effectiveLineSpacing = fm.lineSpacing();
    if (effectiveLineSpacing <= 0.0) {
        effectiveLineSpacing = fm.height() > 0.0 ? fm.height() : (effectivePointSize * 1.4);
    }
    
    m_effectiveFontPointSize = effectivePointSize;
    m_effectiveFontLineSpacing = effectiveLineSpacing;
    
    qreal baseLs = m_baseFontLineSpacing > 0.0 ? m_baseFontLineSpacing : 14.0;
    m_fontMetricsScale = std::clamp(effectiveLineSpacing / baseLs, 0.5, 3.0);

    // Playlist font metrics
    QFont plFont = font;
    if (!m_playlistFontFamily.isEmpty() && m_playlistFontFamily != QStringLiteral("Default")) {
        const QStringList families = QFontDatabase::families();
        if (families.contains(m_playlistFontFamily)) {
            plFont.setFamily(m_playlistFontFamily);
        }
    }
    QFontMetricsF plFm(plFont);
    qreal plLineSpacing = plFm.lineSpacing();
    if (plLineSpacing <= 0.0) {
        plLineSpacing = plFm.height() > 0.0 ? plFm.height() : (effectivePointSize * 1.4);
    }
    m_playlistFontLineSpacing = plLineSpacing;
    m_playlistFontMetricsScale = std::clamp(plLineSpacing / baseLs, 0.5, 3.0);
    
    emit fontFamilyChanged();
    emit fontSizeMultiplierChanged();
    emit fontMetricsScaleChanged();
    emit baseFontPointSizeChanged();
    emit effectiveFontPointSizeChanged();
    emit effectiveFontLineSpacingChanged();
    emit baseFontLineSpacingChanged();
    emit playlistFontFamilyChanged();
    emit playlistFontMetricsScaleChanged();
    emit playlistFontLineSpacingChanged();
    emit themeChanged();
}

QString ThemeManager::playlistFontFamily() const
{
    if (!m_playlistFontFamily.isEmpty() && m_playlistFontFamily != QStringLiteral("Default")) {
        return m_playlistFontFamily;
    }
    return fontFamily();
}

void ThemeManager::setPlaylistFontFamily(const QString &family)
{
    if (m_playlistFontFamily != family) {
        m_playlistFontFamily = family;
        emit playlistFontFamilyChanged();
        updateApplicationFont();
    }
}
