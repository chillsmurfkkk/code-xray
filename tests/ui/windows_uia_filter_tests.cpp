#include "app/windows_uia_filter.hpp"

#include <QCoreApplication>

#include <iostream>

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    xray::app::WindowsUiaFilter filter;
    MSG message{};
    message.message = WM_GETOBJECT;
    qintptr result = 42;

    if (!filter.nativeEventFilter("windows_generic_MSG", &message, &result) || result != 0) {
        std::cerr << "WM_GETOBJECT must be intercepted in the explicit workaround mode\n";
        return 1;
    }

    result = 42;
    if (!filter.nativeEventFilter("windows_dispatcher_MSG", &message, &result) || result != 0) {
        std::cerr << "Dispatcher messages must follow the same rule\n";
        return 1;
    }

    result = 42;
    message.message = WM_CLOSE;
    if (filter.nativeEventFilter("windows_generic_MSG", &message, &result) || result != 42) {
        std::cerr << "Other Windows messages must pass through unchanged\n";
        return 1;
    }

    message.message = WM_GETOBJECT;
    if (filter.nativeEventFilter("other_platform", &message, &result)
        || filter.nativeEventFilter("windows_generic_MSG", nullptr, &result)
        || filter.nativeEventFilter("windows_generic_MSG", &message, nullptr)
        || result != 42) {
        std::cerr << "Unsupported or incomplete events must pass through\n";
        return 1;
    }

    std::cout << "Windows UI Automation filter: passed\n";
    return 0;
}
