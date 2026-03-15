#include "ThemeManager.h"
#include <QStyleHints>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_settings("WaveFlux", "WaveFlux")
{
    // Initialize with system palette
    applySystemPalette();
    loadSettings();
    
    // Connect to system theme changes
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this]() {
        applySystemPalette();
        emit themeChanged();
    });
}

ThemeManager::~ThemeManager()
{
    saveSettings();
}

void ThemeManager::setWaveformColor(const QColor &color)
{
    if (m_waveformColor != color) {
        m_waveformColor = color;
        emit waveformColorChanged();
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
        m_progressColor = QColor(m_settings.value("progressColor").toString());
        m_accentColor = QColor(m_settings.value("accentColor").toString());
        m_primaryColor = QColor(m_settings.value("primaryColor", m_accentColor.name()).toString());
    }
    
    m_settings.endGroup();
    
    emit waveformColorChanged();
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

    emit waveformColorChanged();
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

void ThemeManager::loadSettings()
{
    m_settings.beginGroup("Theme");

    if (m_settings.contains("waveformColor")) {
        m_waveformColor = QColor(m_settings.value("waveformColor").toString());
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
    m_settings.endGroup();
}

void ThemeManager::saveSettings()
{
    m_settings.beginGroup("Theme");
    // Only save user-customizable colors (waveform, progress, accent)
    m_settings.setValue("waveformColor", m_waveformColor.name());
    m_settings.setValue("progressColor", m_progressColor.name());
    m_settings.setValue("primaryColor", m_primaryColor.name());
    m_settings.setValue("accentColor", m_accentColor.name());
    m_settings.endGroup();
    m_settings.sync();
}

void ThemeManager::applyDarkTheme()
{
    applySystemPalette();
    m_darkMode = true;
    
    emit waveformColorChanged();
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
    
    // Detect dark mode from system
    m_darkMode = (palette.color(QPalette::Window).lightness() < 128);
}
