#pragma once

#include <QMainWindow>

namespace xray::app {

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
};

} // namespace xray::app
