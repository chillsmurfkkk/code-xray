#pragma once

#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN
#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QtCore/qt_windows.h>

namespace xray::app {

// Explicit, temporary workaround for a Qt/Windows UI Automation hang.
// Installing this filter prevents accessibility clients from accessing Qt
// widgets in this process; leave it uninstalled for the normal launch.
class WindowsUiaFilter final : public QAbstractNativeEventFilter {
public:
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override {
        if ((eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
            || !message || !result) {
            return false;
        }

        const auto* nativeMessage = static_cast<const MSG*>(message);
        if (nativeMessage->message != WM_GETOBJECT) {
            return false;
        }

        *result = 0;
        return true;
    }
};

} // namespace xray::app
#endif
