#pragma once

class QApplication;

namespace xray::ui {

enum class Theme {
    dark,
    light
};

Theme loadTheme();
void saveTheme(Theme theme);
void applyTheme(QApplication& application, Theme theme);

} // namespace xray::ui
