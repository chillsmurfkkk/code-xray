#pragma once

#include <QPlainTextEdit>

#include <vector>

class QEvent;
class QPaintEvent;
class QResizeEvent;

namespace xray::ui {

class LineNumberArea;
class CppSyntaxHighlighter;

class CodeView final : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit CodeView(QWidget* parent = nullptr);

    // Accepts decoded text with BOM removed and CRLF normalized to LF.
    void setSourceText(const QString& text);

    [[nodiscard]]
    const std::vector<int>& sourceLineStarts() const noexcept {
        return sourceLineStarts_;
    }

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    friend class LineNumberArea;

    [[nodiscard]]
    int lineNumberAreaWidth() const;

    void updateLineNumberAreaGeometry();
    void paintLineNumberArea(QPaintEvent* event);

    LineNumberArea* lineNumberArea_ = nullptr;
    CppSyntaxHighlighter* highlighter_ = nullptr;
    std::vector<int> sourceLineStarts_{0};
};

} // namespace xray::ui
