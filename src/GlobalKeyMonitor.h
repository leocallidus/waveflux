#ifndef GLOBALKEYMONITOR_H
#define GLOBALKEYMONITOR_H

#include <QObject>
#include <QPointer>

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

signals:
    void plainSpacePressed();
    void plainSpaceReleased();
    void plainSpaceCanceled();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static bool isPlainSpaceEvent(const QKeyEvent *event);
    static bool focusObjectAcceptsTextInput(QObject *object);
    bool shouldHandlePlainSpace(const QKeyEvent *event) const;
    void cancelTrackedSpacePress();

    QPointer<QWindow> m_mainWindow;
    bool m_plainSpacePressed = false;
};

#endif // GLOBALKEYMONITOR_H
