#include "ui/code/structure_page.hpp"
#include "ui/shared/popup_style.hpp"
#include "ui/shared/theme.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QImage>
#include <QKeyEvent>
#include <QToolTip>

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class HiddenWindows final : public QObject {
    bool eventFilter(QObject* object, QEvent* event) override {
        if (event->type() == QEvent::Polish) {
            if (auto* widget = qobject_cast<QWidget*>(object); widget && widget->isWindow())
                widget->setAttribute(Qt::WA_DontShowOnScreen);
        }
        return false;
    }
};

void checkMask(const QWidget& widget) {
    const auto mask = widget.mask();
    const auto rect = widget.rect();
    require(!mask.isEmpty() && mask.contains(rect.center()), "Popup mask must preserve its center");
    for (const auto& corner : {rect.topLeft(), rect.topRight(), rect.bottomLeft(), rect.bottomRight()})
        require(!mask.contains(corner), "Popup mask must clip every rectangular corner");
}

void sendKey(QWidget& widget, int key) {
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
    QApplication::sendEvent(&widget, &press);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
    QApplication::sendEvent(&widget, &release);
}
} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    HiddenWindows hidden;
    app.installEventFilter(&hidden);
    xray::ui::installPopupStyle(app);
    const auto output = app.arguments().value(1);
    if (!output.isEmpty()) QDir().mkpath(output);

    try {
        require(!app.isEffectEnabled(Qt::UI_AnimateTooltip)
                    && !app.isEffectEnabled(Qt::UI_FadeTooltip)
                    && !app.isEffectEnabled(Qt::UI_AnimateCombo),
                "Popup snapshot animations must remain disabled");
        xray::ui::applyTheme(app, xray::ui::Theme::dark);
        xray::ui::StructurePage page;
        page.resize(920, 600);
        page.show();
        auto* combo = page.findChild<QComboBox*>("entityKindFilter");
        require(combo, "Missing production entity filter");

        for (const auto theme : {xray::ui::Theme::dark, xray::ui::Theme::light}) {
            xray::ui::applyTheme(app, theme);
            const QString name = theme == xray::ui::Theme::dark ? "dark" : "light";
            combo->setCurrentIndex(0);
            auto* popup = combo->view()->window();
            popup->setAttribute(Qt::WA_DontShowOnScreen);
            combo->showPopup();
            app.processEvents();
            checkMask(*popup);
            require(combo->view()->sizeHintForRow(0) >= combo->fontMetrics().height() + 12,
                    "Popup row padding must remain styled after a theme change");
            const auto image = popup->grab().toImage();
            require(image.pixelColor(0, 0).alpha() == 0, "Combo corners must have no painted native frame");
            if (!output.isEmpty())
                require(image.save(output + "/combo-" + name + ".png"), "Cannot render popup preview");
            popup->resize(popup->width() + 31, popup->height() + 13);
            app.processEvents();
            checkMask(*popup);
            sendKey(*combo->view(), Qt::Key_Down);
            sendKey(*combo->view(), Qt::Key_Return);
            require(combo->currentIndex() == 1 && !popup->isVisible(),
                    "Keyboard selection must still select and close the popup");
            combo->showPopup();
            sendKey(*combo->view(), Qt::Key_Escape);
            require(!popup->isVisible(), "Escape must close the popup");

            QToolTip::showText(QPoint(100, 100), "Popup shape check", &page);
            app.processEvents();
            QWidget* tooltip = nullptr;
            for (auto* widget : app.topLevelWidgets()) {
                if (widget->windowType() == Qt::ToolTip && widget->isVisible()) {
                    require(!tooltip, "Tooltip animations must not create a second visible window");
                    tooltip = widget;
                }
            }
            require(tooltip, "Qt must create the tooltip window");
            checkMask(*tooltip);
            tooltip->resize(tooltip->width() + 23, tooltip->height() + 7);
            checkMask(*tooltip);
            tooltip->hide();
        }
        std::cout << "Popup masks, corners, theme changes, resizing and keyboard interaction: passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
