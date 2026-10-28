#include "code_view.hpp"
#include "cpp_syntax_highlighter.hpp"

#include <QApplication>
#include <QPaintEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QPainter>
#include <QPalette>
#include <QTextBlock>
#include <QTextCursor>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace xray::ui {

class LineNumberArea final : public QWidget {
public:
    explicit LineNumberArea(CodeView* view)
        : QWidget(view),
          view_(view) {
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        view_->paintLineNumberArea(event);
    }

private:
    CodeView* view_;
};

CodeView::CodeView(QWidget* parent)
    : QPlainTextEdit(parent) {
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabChangesFocus(true);

    lineNumberArea_ = new LineNumberArea(this);
    highlighter_ = new CppSyntaxHighlighter(document(), QApplication::palette());

    connect(
        this,
        &QPlainTextEdit::updateRequest,
        this,
        [this](const QRect&, int) {
            lineNumberArea_->update();
        }
    );

    updateLineNumberAreaGeometry();
}

int CodeView::lineNumberAreaWidth() const {
    const auto digits = QString::number(
        static_cast<qulonglong>(sourceLineStarts_.size())
    ).size();

    const QString numberTemplate(digits, QLatin1Char('9'));

    return 16 + fontMetrics().horizontalAdvance(numberTemplate);
}

void CodeView::updateLineNumberAreaGeometry() {
    if (!lineNumberArea_) {
        return;
    }

    const int areaWidth = lineNumberAreaWidth();

    if (viewportMargins().left() != areaWidth) {
        setViewportMargins(areaWidth, 0, 0, 0);
    }

    const QRect textArea = viewport()->geometry();

    lineNumberArea_->setGeometry(
        textArea.left() - areaWidth,
        textArea.top(),
        areaWidth,
        textArea.height()
    );
}

void CodeView::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);

    updateLineNumberAreaGeometry();
}

void CodeView::changeEvent(QEvent* event) {
    QPlainTextEdit::changeEvent(event);

    if (!lineNumberArea_) {
        return;
    }

    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
        updateLineNumberAreaGeometry();
        lineNumberArea_->update();
        // Application QSS can temporarily retain a widget's old Window role
        // during repolishing. Syntax follows the shared application theme.
        if (highlighter_) highlighter_->setPalette(QApplication::palette());
        break;

    default:
        break;
    }
}

void CodeView::setSourceText(const QString& text) {
    if (text.size() >= std::numeric_limits<int>::max()) {
        throw std::length_error(
            "Source text exceeds Qt document position limits"
        );
    }

    sourceLineStarts_.assign(1, 0);

    for (qsizetype position = 0; position < text.size(); ++position) {
        if (text.at(position) == QLatin1Char('\n')) {
            sourceLineStarts_.push_back(
                static_cast<int>(position + 1)
            );
        }
    }

    highlighter_->setSourceLineStarts(sourceLineStarts_);
    QPlainTextEdit::setPlainText(text);

    updateLineNumberAreaGeometry();
    lineNumberArea_->update();
}

void CodeView::paintLineNumberArea(QPaintEvent* event) {
    QPainter painter(lineNumberArea_);

    painter.fillRect(
        event->rect(),
        palette().brush(QPalette::Base)
    );
    painter.setPen(
        palette().color(QPalette::PlaceholderText)
    );
    painter.setFont(font());

    const int firstVisiblePosition = firstVisibleBlock().position();

    auto line = std::upper_bound(
        sourceLineStarts_.begin(),
        sourceLineStarts_.end(),
        firstVisiblePosition
    );

    if (line != sourceLineStarts_.begin()) {
        --line;
    }

    QTextCursor cursor(document());

    for (; line != sourceLineStarts_.end(); ++line) {
        cursor.setPosition(*line);

        const QRect textRect = cursorRect(cursor);

        if (textRect.top() > event->rect().bottom()) {
            break;
        }

        if (textRect.bottom() < event->rect().top()) {
            continue;
        }

        const auto number = static_cast<qulonglong>(
            line - sourceLineStarts_.begin() + 1
        );

        painter.drawText(
            QRect(
                8,
                textRect.top(),
                lineNumberArea_->width() - 16,
                textRect.height()
            ),
            Qt::AlignRight | Qt::AlignVCenter,
            QString::number(number)
        );
    }
}

} // namespace xray::ui
