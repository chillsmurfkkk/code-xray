#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QStringList>

#include <vector>

class QPalette;
class QTextDocument;

namespace xray::ui {

class CppSyntaxHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit CppSyntaxHighlighter(
        QTextDocument* document,
        const QPalette& palette
    );

    void setPalette(const QPalette& palette);

    // Call before replacing the document text. Positions refer to decoded,
    // BOM-free, LF-normalized source, as in CodeView::sourceLineStarts().
    void setSourceLineStarts(const std::vector<int>& starts);

protected:
    void highlightBlock(const QString& text) override;

private:
    QTextCharFormat keywordFormat_;
    QTextCharFormat numberFormat_;
    QTextCharFormat stringFormat_;
    QTextCharFormat commentFormat_;
    QTextCharFormat preprocessorFormat_;
    QStringList rawStringEndings_;
    std::vector<int> sourceLineStarts_;
};

} // namespace xray::ui
