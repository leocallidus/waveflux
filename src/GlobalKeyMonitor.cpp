#include "GlobalKeyMonitor.h"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMetaObject>
#include <QVariant>
#include <QWindow>

namespace {
bool metaObjectChainContains(const QObject *object, const char *needle)
{
    if (!object || !needle || *needle == '\0') {
        return false;
    }

    const QByteArray token(needle);
    const QMetaObject *metaObject = object->metaObject();
    while (metaObject) {
        if (QByteArray(metaObject->className()).contains(token)) {
            return true;
        }
        metaObject = metaObject->superClass();
    }

    return false;
}
}

GlobalKeyMonitor::GlobalKeyMonitor(QObject *parent)
    : QObject(parent)
{
    if (qApp) {
        qApp->installEventFilter(this);
    }
}

GlobalKeyMonitor::~GlobalKeyMonitor()
{
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void GlobalKeyMonitor::setMainWindow(QWindow *window)
{
    if (m_mainWindow == window) {
        return;
    }

    cancelTrackedTapHoldPress();
    m_mainWindow = window;
}

void GlobalKeyMonitor::setTapHoldShortcutSequence(const QString &sequence)
{
    QKeySequence keySequence = QKeySequence::fromString(sequence.trimmed(), QKeySequence::PortableText);
    if (keySequence.isEmpty()) {
        keySequence = QKeySequence::fromString(sequence.trimmed(), QKeySequence::NativeText);
    }

    int nextKey = 0;
    Qt::KeyboardModifiers nextModifiers = Qt::NoModifier;
    if (!keySequence.isEmpty() && keySequence.count() == 1) {
        const QKeyCombination combination = keySequence[0];
        nextKey = combination.key();
        nextModifiers = shortcutRelevantModifiers(combination.keyboardModifiers());
    }

    if (m_tapHoldKey == nextKey && m_tapHoldModifiers == nextModifiers) {
        return;
    }

    cancelTrackedTapHoldPress();
    m_tapHoldKey = nextKey;
    m_tapHoldModifiers = nextModifiers;
}

bool GlobalKeyMonitor::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)

    if (!event) {
        return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::ApplicationDeactivate
        || event->type() == QEvent::WindowDeactivate) {
        cancelTrackedTapHoldPress();
        return QObject::eventFilter(watched, event);
    }

    auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
    if (!keyEvent) {
        return QObject::eventFilter(watched, event);
    }

    // When a tap-hold press is active, suppress auto-repeat events for the
    // same key so they never reach focused controls (e.g. ToolButtons that
    // would otherwise activate on Space).
    if (m_tapHoldPressed && isAutoRepeatOfTrackedKey(keyEvent)) {
        keyEvent->accept();
        return true;
    }

    if (!shouldHandleTapHoldShortcut(keyEvent)) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::ShortcutOverride:
        keyEvent->accept();
        return true;
    case QEvent::KeyPress:
        if (!m_tapHoldPressed) {
            m_tapHoldPressed = true;
            emit plainSpacePressed();
            emit tapHoldShortcutPressed();
        }
        keyEvent->accept();
        return true;
    case QEvent::KeyRelease:
        if (m_tapHoldPressed) {
            m_tapHoldPressed = false;
            emit plainSpaceReleased();
            emit tapHoldShortcutReleased();
        }
        keyEvent->accept();
        return true;
    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}

Qt::KeyboardModifiers GlobalKeyMonitor::shortcutRelevantModifiers(Qt::KeyboardModifiers modifiers)
{
    return modifiers & (Qt::ControlModifier
                        | Qt::ShiftModifier
                        | Qt::AltModifier
                        | Qt::MetaModifier
                        | Qt::KeypadModifier);
}

bool GlobalKeyMonitor::isTapHoldShortcutEvent(const QKeyEvent *event) const
{
    if (!event || m_tapHoldKey == 0 || event->isAutoRepeat()) {
        return false;
    }

    return event->key() == m_tapHoldKey
        && shortcutRelevantModifiers(event->modifiers()) == m_tapHoldModifiers;
}

bool GlobalKeyMonitor::isAutoRepeatOfTrackedKey(const QKeyEvent *event) const
{
    if (!event || m_tapHoldKey == 0 || !event->isAutoRepeat()) {
        return false;
    }

    return event->key() == m_tapHoldKey
        && shortcutRelevantModifiers(event->modifiers()) == m_tapHoldModifiers;
}

bool GlobalKeyMonitor::focusObjectAcceptsTextInput(QObject *object)
{
    for (QObject *current = object; current; current = current->parent()) {
        if (metaObjectChainContains(current, "TextInput")
            || metaObjectChainContains(current, "TextEdit")
            || metaObjectChainContains(current, "SpinBox")) {
            return true;
        }

        const QVariant readOnly = current->property("readOnly");
        if (readOnly.isValid() && !readOnly.toBool()) {
            if (current->property("echoMode").isValid()
                || current->property("inputMethodComposing").isValid()
                || current->property("acceptableInput").isValid()) {
                return true;
            }
        }

        const QVariant editable = current->property("editable");
        if (editable.isValid() && editable.toBool()) {
            return true;
        }
    }

    return false;
}

bool GlobalKeyMonitor::shouldHandleTapHoldShortcut(const QKeyEvent *event) const
{
    if (!m_mainWindow || !isTapHoldShortcutEvent(event)) {
        return false;
    }

    if (QGuiApplication::focusWindow() != m_mainWindow) {
        return false;
    }

    return !focusObjectAcceptsTextInput(QGuiApplication::focusObject());
}

void GlobalKeyMonitor::cancelTrackedTapHoldPress()
{
    if (!m_tapHoldPressed) {
        return;
    }

    m_tapHoldPressed = false;
    emit plainSpaceCanceled();
    emit tapHoldShortcutCanceled();
}
