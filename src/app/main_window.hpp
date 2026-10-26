#pragma once

#include <QMainWindow>

#include "ui/shared/theme.hpp"

namespace xray::app {

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(ui::Theme theme, QWidget* parent = nullptr);

private:
    ui::Theme theme_;
};

} // namespace xray::app
