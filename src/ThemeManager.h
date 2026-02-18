#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QColor>
#include <QSettings>
#include <QPalette>
#include <QGuiApplication>
#include <QFont>
#include <QFontDatabase>

/**
 * @brief ThemeManager - Manages color themes and settings
 * 
 * Provides theming support for the UI, allowing customization of:
 * - Waveform colors
 * - Background colors
 * - Accent colors
 * Settings are persisted using QSettings.
 */
class ThemeManager : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QColor waveformColor READ waveformColor WRITE setWaveformColor NOTIFY waveformColorChanged)
    Q_PROPERTY(QColor progressColor READ progressColor WRITE setProgressColor NOTIFY progressColorChanged)
    Q_PROPERTY(QColor primaryColor READ primaryColor WRITE setPrimaryColor NOTIFY primaryColorChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor WRITE setSurfaceColor NOTIFY surfaceColorChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY textColorChanged)
    Q_PROPERTY(QColor textSecondaryColor READ textSecondaryColor WRITE setTextSecondaryColor NOTIFY textSecondaryColorChanged)
    Q_PROPERTY(QColor textMutedColor READ textMutedColor WRITE setTextMutedColor NOTIFY textMutedColorChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)
    Q_PROPERTY(QString monoFontFamily READ monoFontFamily CONSTANT)
    Q_PROPERTY(int spacingSmall READ spacingSmall CONSTANT)
    Q_PROPERTY(int spacingMedium READ spacingMedium CONSTANT)
    Q_PROPERTY(int spacingLarge READ spacingLarge CONSTANT)
    Q_PROPERTY(int borderRadius READ borderRadius CONSTANT)
    Q_PROPERTY(int borderRadiusLarge READ borderRadiusLarge CONSTANT)
    Q_PROPERTY(int headerHeight READ headerHeight CONSTANT)
    Q_PROPERTY(int footerHeight READ footerHeight CONSTANT)
    Q_PROPERTY(int sidebarWidth READ sidebarWidth CONSTANT)
    Q_PROPERTY(bool darkMode READ isDarkMode WRITE setDarkMode NOTIFY darkModeChanged)
    
public:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager() override;
    
    QColor waveformColor() const { return m_waveformColor; }
    void setWaveformColor(const QColor &color);
    
    QColor progressColor() const { return m_progressColor; }
    void setProgressColor(const QColor &color);

    QColor primaryColor() const { return m_primaryColor; }
    void setPrimaryColor(const QColor &color);
    
    QColor backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const QColor &color);

    QColor surfaceColor() const { return m_surfaceColor; }
    void setSurfaceColor(const QColor &color);

    QColor borderColor() const { return m_borderColor; }
    void setBorderColor(const QColor &color);
    
    QColor accentColor() const { return m_accentColor; }
    void setAccentColor(const QColor &color);
    
    QColor textColor() const { return m_textColor; }
    void setTextColor(const QColor &color);

    QColor textSecondaryColor() const { return m_textSecondaryColor; }
    void setTextSecondaryColor(const QColor &color);

    QColor textMutedColor() const { return m_textMutedColor; }
    void setTextMutedColor(const QColor &color);

    QString fontFamily() const { return QGuiApplication::font().family(); }
    QString monoFontFamily() const { return QFontDatabase::systemFont(QFontDatabase::FixedFont).family(); }
    int spacingSmall() const { return 6; }
    int spacingMedium() const { return 8; }
    int spacingLarge() const { return 12; }
    int borderRadius() const { return 4; }
    int borderRadiusLarge() const { return 8; }
    int headerHeight() const { return 40; }
    int footerHeight() const { return 140; }
    int sidebarWidth() const { return 256; }
    
    bool isDarkMode() const { return m_darkMode; }
    void setDarkMode(bool dark);
    
    Q_INVOKABLE void loadTheme(const QString &name);
    Q_INVOKABLE void saveCurrentTheme(const QString &name);
    Q_INVOKABLE QStringList availableThemes() const;
    Q_INVOKABLE void resetToDefault();
    
signals:
    void waveformColorChanged();
    void progressColorChanged();
    void primaryColorChanged();
    void backgroundColorChanged();
    void surfaceColorChanged();
    void borderColorChanged();
    void accentColorChanged();
    void textColorChanged();
    void textSecondaryColorChanged();
    void textMutedColorChanged();
    void darkModeChanged();
    void themeChanged();
    
private:
    void loadSettings();
    void saveSettings();
    void applyDarkTheme();
    void applyLightTheme();
    void applySystemPalette();
    
    QSettings m_settings;
    
    QColor m_waveformColor;
    QColor m_progressColor;
    QColor m_primaryColor;
    QColor m_backgroundColor;
    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_accentColor;
    QColor m_textColor;
    QColor m_textSecondaryColor;
    QColor m_textMutedColor;
    bool m_darkMode = true;
};

#endif // THEMEMANAGER_H
