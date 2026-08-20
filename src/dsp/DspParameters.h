#ifndef DSPPARAMETERS_H
#define DSPPARAMETERS_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <vector>

namespace WaveFlux::Dsp {

enum class ParameterType {
    Double,
    Int,
    Bool,
    StringEnum
};

struct ParameterDefinition {
    QString id;
    QString tabId;
    QString groupId;
    QString nameKey;
    QString descriptionKey;
    ParameterType type = ParameterType::Double;
    double minValue = 0.0;
    double maxValue = 1.0;
    double step = 0.01;
    double neutralValue = 0.0;
    double defaultValue = 0.0;
    QString unit;
    QStringList enumValues;

    QVariantMap toVariantMap() const;
};

// Access the complete static catalog of DSP parameter definitions
const std::vector<ParameterDefinition> &allParameterDefinitions();

// Look up a definition by ID (returns nullptr if unknown)
const ParameterDefinition *findParameterDefinition(const QString &id);

// Get list of definitions belonging to a specific tab
std::vector<ParameterDefinition> parametersForTab(const QString &tabId);

// Get list of definitions belonging to a specific group
std::vector<ParameterDefinition> parametersForGroup(const QString &groupId);

// Validation / normalization helpers
double sanitizeDouble(const ParameterDefinition &def, double value);
int sanitizeInt(const ParameterDefinition &def, int value);
QString sanitizeEnum(const ParameterDefinition &def, const QString &value);
QVariant sanitizeValue(const ParameterDefinition &def, const QVariant &value);

} // namespace WaveFlux::Dsp

#endif // DSPPARAMETERS_H
