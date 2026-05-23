#ifndef SHORTCUTREGISTRY_H
#define SHORTCUTREGISTRY_H

#include <QString>
#include <QStringList>
#include <QVector>

struct ShortcutDefinition
{
    QString id;
    QString translationKey;
    QString group;
    QString context;
    QString defaultSequence;
    bool userAssignable = true;
    bool allowEmpty = true;
    QStringList reservedSequences;
    QString source;
    QString notes;
};

class ShortcutRegistry
{
public:
    static QVector<ShortcutDefinition> definitions();
    static const ShortcutDefinition *findDefinition(const QString &id);
};

#endif // SHORTCUTREGISTRY_H
