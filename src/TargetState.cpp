/**
 * TargetState implementation - X11/Wayland target capture
 */
#include "TargetState.h"
#include <QtCore/QDebug>
#include <QtWidgets/QApplication>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

TargetState::TargetState(QObject* parent) : QObject(parent) {}

bool TargetState::capture()
{
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        qWarning() << "Failed to open X display for target capture";
        return false;
    }

    Window root = DefaultRootWindow(display);
    Window focus;
    int revert_to;
    XGetInputFocus(display, &focus, &revert_to);

    if (focus == None || focus == root) {
        qWarning() << "No focused window (focus: " << focus << ")";
        XCloseDisplay(display);
        return false;
    }

    m_targetWindow = focus;

    // Get window geometry to approximate caret position
    XWindowAttributes attr;
    if (XGetWindowAttributes(display, focus, &attr)) {
        // Use window center as fallback caret position
        m_caretScreenPos = QPoint(attr.x + attr.width / 2, attr.y + attr.height / 2);
    } else {
        // Fallback to cursor position
        m_caretScreenPos = QCursor::pos();
    }

    XCloseDisplay(display);
    m_valid = true;
   	qDebug() << "Captured target: window=" << m_targetWindow << "caret=" << m_caretScreenPos;
    return true;
}

bool TargetState::isValid() const
{
    if (!m_valid || m_targetWindow == 0) return false;

    // Verify window still exists
    Display* display = XOpenDisplay(nullptr);
    if (!display) return false;

    XWindowAttributes attr;
    bool ok = XGetWindowAttributes(display, m_targetWindow, &attr) != 0;
    XCloseDisplay(display);
    return ok;
}

bool TargetState::ensureForeground()
{
    Display* display = XOpenDisplay(nullptr);
    if (!display) return false;

    XRaiseWindow(display, m_targetWindow);
    XSetInputFocus(display, m_targetWindow, RevertToParent, CurrentTime);
    XFlush(display);
    XCloseDisplay(display);
    return true;
}

void TargetState::clear()
{
    m_targetWindow = 0;
    m_caretScreenPos = QPoint();
    m_valid = false;
}