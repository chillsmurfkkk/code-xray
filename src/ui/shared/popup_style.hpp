#pragma once

#include <QApplication>
#include <QPainterPath>
#include <QProxyStyle>
#include <QRegion>
#include <QStyleFactory>
#include <QStyleOption>

namespace xray::ui {

// QSS rounds painting, but top-level popup windows also need a matching mask.
class PopupStyle final : public QProxyStyle {
public:
    PopupStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}

    int styleHint(
        StyleHint hint,
        const QStyleOption* option = nullptr,
        const QWidget* widget = nullptr,
        QStyleHintReturn* returnData = nullptr
    ) const override {
        if ((hint == SH_Menu_Mask || hint == SH_ToolTip_Mask) && option) {
            if (auto* mask = qstyleoption_cast<QStyleHintReturnMask*>(returnData)) {
                QPainterPath outline;
                outline.addRoundedRect(QRectF(option->rect), 8, 8);
                mask->region = QRegion(outline.toFillPolygon().toPolygon());
                return 1;
            }
        }
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

inline void installPopupStyle(QApplication& application) {
    application.setStyle(new PopupStyle);
    // Qt's animation helpers use separate rectangular snapshot windows.
    application.setEffectEnabled(Qt::UI_AnimateTooltip, false);
    application.setEffectEnabled(Qt::UI_FadeTooltip, false);
    application.setEffectEnabled(Qt::UI_AnimateCombo, false);
}

} // namespace xray::ui
