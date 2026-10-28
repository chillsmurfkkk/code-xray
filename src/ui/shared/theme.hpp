#pragma once

#include <QColor>

class QApplication;
class QPalette;

namespace xray::ui {

enum class Theme {
    dark,
    light
};

struct SyntaxColors {
    QColor keyword;
    QColor number;
    QColor stringLiteral;
    QColor comment;
    QColor preprocessor;
};

SyntaxColors syntaxColors(const QPalette& palette);

Theme loadTheme();
void saveTheme(Theme theme);
void applyTheme(QApplication& application, Theme theme);

} // namespace xray::ui
