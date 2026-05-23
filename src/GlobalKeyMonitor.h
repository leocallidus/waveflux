#ifndef GLOBALKEYMONITOR_H
#define GLOBALKEYMONITOR_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <Qt>

class QEvent;
class QKeyEvent;
class QWindow;

class GlobalKeyMonitor : public QObject
{
    Q_OBJECT

public:
    explicit GlobalKeyMonitor(QObject *parent = nullptr);
    ~GlobalKeyMonitor() override;

    void setMainWindow(QWindow *window);
    void setTapHoldShortcutSequence(const QString &sequence);

signals:
    void plainSpacePressed();
    void plainSpaceReleased();
    void plainSpaceCanceled();
    void tapHoldShortcutPressed();
    void tapHoldShortcutReleased();
    void tapHoldShortcutCanceled();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static bool focusObjectAcceptsTextInput(QObject *object);
    static Qt::KeyboardModifiers shortcutRelevantModifiers(Qt::KeyboardModifiers modifiers);
    bool isTapHoldShortcutEvent(const QKeyEvent *event) const;
    bool isAutoRepeatOfTrackedKey(const QKeyEvent *event) const;
    bool shouldHandleTapHoldShortcut(const QKeyEvent *event) const;
    void cancelTrackedTapHoldPress();

    QPointer<QWindow> m_mainWindow;
    int m_tapHoldKey = Qt::Key_Space;
    Qt::KeyboardModifiers m_tapHoldModifiers = Qt::NoModifier;
    bool m_tapHoldPressed = false;
};

#endif // GLOBALKEYMONITOR_H
