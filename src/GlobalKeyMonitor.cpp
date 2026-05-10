#include "GlobalKeyMonitor.h"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
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

    cancelTrackedSpacePress();
    m_mainWindow = window;
}

bool GlobalKeyMonitor::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)

    if (!event) {
        return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::ApplicationDeactivate
        || event->type() == QEvent::WindowDeactivate) {
        cancelTrackedSpacePress();
        return QObject::eventFilter(watched, event);
    }

    auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
    if (!keyEvent || !shouldHandlePlainSpace(keyEvent)) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::ShortcutOverride:
        keyEvent->accept();
        return true;
    case QEvent::KeyPress:
        if (!m_plainSpacePressed) {
            m_plainSpacePressed = true;
            emit plainSpacePressed();
        }
        keyEvent->accept();
        return true;
    case QEvent::KeyRelease:
        if (m_plainSpacePressed) {
            m_plainSpacePressed = false;
            emit plainSpaceReleased();
        }
        keyEvent->accept();
        return true;
    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}

bool GlobalKeyMonitor::isPlainSpaceEvent(const QKeyEvent *event)
{
    if (!event || event->key() != Qt::Key_Space || event->isAutoRepeat()) {
        return false;
    }

    const Qt::KeyboardModifiers modifiers = event->modifiers();
    return (modifiers & Qt::ControlModifier) == 0
        && (modifiers & Qt::ShiftModifier) == 0
        && (modifiers & Qt::AltModifier) == 0
        && (modifiers & Qt::MetaModifier) == 0;
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

bool GlobalKeyMonitor::shouldHandlePlainSpace(const QKeyEvent *event) const
{
    if (!m_mainWindow || !isPlainSpaceEvent(event)) {
        return false;
    }

    if (QGuiApplication::focusWindow() != m_mainWindow) {
        return false;
    }

    return !focusObjectAcceptsTextInput(QGuiApplication::focusObject());
}

void GlobalKeyMonitor::cancelTrackedSpacePress()
{
    if (!m_plainSpacePressed) {
        return;
    }

    m_plainSpacePressed = false;
    emit plainSpaceCanceled();
}
