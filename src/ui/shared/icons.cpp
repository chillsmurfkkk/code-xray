#include "icons.hpp"

#include <QApplication>
#include <QEvent>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QToolTip>

namespace xray::ui {
namespace {

class SymbolEngine final : public QIconEngine {
public:
    explicit SymbolEngine(Symbol symbol) : symbol_(symbol) {}
    QIconEngine* clone() const override { return new SymbolEngine(symbol_); }

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode,
               QIcon::State state) override {
        const auto palette = QApplication::palette();
        const QColor color = mode == QIcon::Disabled
            ? palette.color(QPalette::Disabled, QPalette::Text)
            : palette.color(state == QIcon::On || mode == QIcon::Selected
                ? QPalette::Accent : QPalette::Text);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const qreal side = qMin(rect.width(), rect.height());
        painter->translate(rect.x() + (rect.width() - side) / 2,
                           rect.y() + (rect.height() - side) / 2);
        painter->scale(side / 24, side / 24);
        painter->setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        QPainterPath path;
        switch (symbol_) {
        case Symbol::code:
            path.moveTo(8, 6); path.lineTo(3, 12); path.lineTo(8, 18);
            path.moveTo(16, 6); path.lineTo(21, 12); path.lineTo(16, 18);
            path.moveTo(14, 4); path.lineTo(10, 20);
            break;
        case Symbol::analysis:
            painter->drawRoundedRect(QRectF(3.5, 3.5, 17, 17), 4, 4);
            path.moveTo(8, 16); path.lineTo(8, 12);
            path.moveTo(12, 16); path.lineTo(12, 7);
            path.moveTo(16, 16); path.lineTo(16, 10);
            break;
        case Symbol::history:
            painter->drawArc(QRectF(4, 4, 16, 16), 35 * 16, -300 * 16);
            path.moveTo(4, 4); path.lineTo(4, 9); path.lineTo(9, 9);
            path.moveTo(12, 7); path.lineTo(12, 12); path.lineTo(16, 14);
            break;
        case Symbol::compare:
            path.moveTo(4, 8); path.lineTo(20, 8); path.lineTo(16, 4);
            path.moveTo(20, 16); path.lineTo(4, 16); path.lineTo(8, 20);
            break;
        case Symbol::source:
            path.moveTo(3, 8); path.lineTo(3, 18); path.quadTo(3, 20, 5, 20);
            path.lineTo(19, 20); path.quadTo(21, 20, 21, 18);
            path.lineTo(21, 8); path.lineTo(12, 8); path.lineTo(10, 5);
            path.lineTo(5, 5); path.quadTo(3, 5, 3, 8);
            break;
        case Symbol::filter:
            path.moveTo(4, 5); path.lineTo(20, 5); path.lineTo(14, 12);
            path.lineTo(14, 19); path.lineTo(10, 21); path.lineTo(10, 12);
            path.closeSubpath();
            break;
        case Symbol::structure:
            painter->drawRoundedRect(QRectF(3, 3, 7, 5), 1.5, 1.5);
            painter->drawRoundedRect(QRectF(14, 10, 7, 5), 1.5, 1.5);
            painter->drawRoundedRect(QRectF(14, 18, 7, 3), 1, 1);
            path.moveTo(6.5, 8); path.lineTo(6.5, 19.5); path.lineTo(14, 19.5);
            path.moveTo(6.5, 12.5); path.lineTo(14, 12.5);
            break;
        case Symbol::diagnostics:
            path.moveTo(10.2, 4.5); path.quadTo(12, 1.5, 13.8, 4.5);
            path.lineTo(21, 17.5); path.quadTo(22.5, 20.5, 19, 20.5);
            path.lineTo(5, 20.5); path.quadTo(1.5, 20.5, 3, 17.5);
            path.closeSubpath();
            path.moveTo(12, 8); path.lineTo(12, 13);
            painter->drawPoint(QPointF(12, 17));
            break;
        case Symbol::sidebarClose:
        case Symbol::sidebarOpen:
            painter->drawRoundedRect(QRectF(3, 4, 18, 16), 3, 3);
            path.moveTo(9, 4); path.lineTo(9, 20);
            if (symbol_ == Symbol::sidebarClose) {
                path.moveTo(16, 9); path.lineTo(13, 12); path.lineTo(16, 15);
            } else {
                path.moveTo(13, 9); path.lineTo(16, 12); path.lineTo(13, 15);
            }
            break;
        case Symbol::appearance:
            if (palette.color(QPalette::Window).lightness() < 128) {
                painter->drawEllipse(QRectF(8, 8, 8, 8));
                painter->save();
                painter->translate(12, 12);
                for (int ray = 0; ray < 8; ++ray) {
                    painter->drawLine(QPointF(0, -8), QPointF(0, -10));
                    painter->rotate(45);
                }
                painter->restore();
            } else {
                path.moveTo(17, 3.8);
                path.cubicTo(6, 1, 0, 15, 9, 20);
                path.cubicTo(14, 23, 20, 19, 20.5, 15);
                path.cubicTo(11, 18, 8, 8, 17, 3.8);
            }
            break;
        case Symbol::document:
            path.moveTo(13, 3); path.lineTo(6, 3); path.quadTo(4, 3, 4, 5);
            path.lineTo(4, 19); path.quadTo(4, 21, 6, 21);
            path.lineTo(18, 21); path.quadTo(20, 21, 20, 19);
            path.lineTo(20, 10); path.lineTo(13, 3); path.lineTo(13, 10);
            path.lineTo(20, 10);
            path.moveTo(8, 14); path.lineTo(16, 14);
            path.moveTo(8, 17); path.lineTo(13, 17);
            break;
        }
        painter->drawPath(path);
        painter->restore();
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
        return scaledPixmap(size, mode, state, 1);
    }
    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode,
                         QIcon::State state, qreal scale) override {
        QPixmap result(size * scale);
        result.setDevicePixelRatio(scale);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        paint(&painter, QRect(QPoint(), size), mode, state);
        return result;
    }
private:
    Symbol symbol_;
};
} // namespace

QIcon makeIcon(Symbol symbol) { return QIcon(new SymbolEngine(symbol)); }

IconButton::IconButton(Symbol symbol, const QString& title, QWidget* parent)
    : QToolButton(parent) {
    setIcon(makeIcon(symbol));
    setIconSize(QSize(20, 20));
    const int extent = qMax(40, fontMetrics().height() + 16);
    setFixedSize(extent, extent);
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setFocusPolicy(Qt::StrongFocus);
    setAutoRaise(true);
    setDescription(title);
}

void IconButton::setDescription(const QString& description) {
    setText(description);
    setToolTip(description);
    setAccessibleName(description);
}

bool IconButton::event(QEvent* event) {
    const bool result = QToolButton::event(event);
    if (event->type() == QEvent::FocusIn && !toolTip().isEmpty())
        QToolTip::showText(mapToGlobal(QPoint(width() + 8, height() / 2)), toolTip(), this);
    else if (event->type() == QEvent::FocusOut)
        QToolTip::hideText();
    return result;
}

SymbolView::SymbolView(Symbol symbol, QWidget* parent) : QWidget(parent), symbol_(symbol) {
    setFixedSize(48, 48);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void SymbolView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    makeIcon(symbol_).paint(&painter, rect().adjusted(10, 10, -10, -10),
                            Qt::AlignCenter, QIcon::Disabled);
}
} // namespace xray::ui
