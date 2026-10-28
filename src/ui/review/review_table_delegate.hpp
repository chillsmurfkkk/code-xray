#pragma once

#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QTableWidget>
#include <QHeaderView>
#include <QStyleOptionViewItem>
#include <QModelIndex>

namespace xray::ui::review {

class RoundedRowDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const bool isSelected = (opt.state & QStyle::State_Selected);
        const bool isHovered = (opt.state & QStyle::State_MouseOver);

        if (isSelected || isHovered) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            if (isSelected) {
                painter->setBrush(opt.palette.brush(QPalette::Highlight));
            } else {
                painter->setBrush(opt.palette.brush(QPalette::AlternateBase));
            }

            const int col = index.column();
            const int totalCols = index.model() ? index.model()->columnCount() : 1;
            const QRectF r = QRectF(opt.rect).adjusted(0, 2, 0, -2);
            const qreal radius = 8.0;

            if (totalCols <= 1) {
                painter->drawRoundedRect(r.adjusted(4, 0, -4, 0), radius, radius);
            } else if (col == 0) {
                QPainterPath path;
                const QRectF cr = r.adjusted(4, 0, 0, 0);
                path.moveTo(cr.right(), cr.top());
                path.lineTo(cr.left() + radius, cr.top());
                path.quadTo(cr.left(), cr.top(), cr.left(), cr.top() + radius);
                path.lineTo(cr.left(), cr.bottom() - radius);
                path.quadTo(cr.left(), cr.bottom(), cr.left() + radius, cr.bottom());
                path.lineTo(cr.right(), cr.bottom());
                path.closeSubpath();
                painter->drawPath(path);
            } else if (col == totalCols - 1) {
                QPainterPath path;
                const QRectF cr = r.adjusted(0, 0, -4, 0);
                path.moveTo(cr.left(), cr.top());
                path.lineTo(cr.right() - radius, cr.top());
                path.quadTo(cr.right(), cr.top(), cr.right(), cr.top() + radius);
                path.lineTo(cr.right(), cr.bottom() - radius);
                path.quadTo(cr.right(), cr.bottom(), cr.right() - radius, cr.bottom());
                path.lineTo(cr.left(), cr.bottom());
                path.closeSubpath();
                painter->drawPath(path);
            } else {
                painter->drawRect(r);
            }
            painter->restore();
        }

        opt.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus | QStyle::State_MouseOver);
        QStyledItemDelegate::paint(painter, opt, index);
    }
};

inline void setupReviewTable(QTableWidget* table) {
    if (!table) return;
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(34);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setMouseTracking(true);
    if (table->viewport()) {
        table->viewport()->setAttribute(Qt::WA_Hover);
    }
    table->setItemDelegate(new RoundedRowDelegate(table));
}

} // namespace xray::ui::review
