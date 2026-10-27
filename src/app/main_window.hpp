#pragma once

#include <QMainWindow>

#include <stop_token>

#include "ui/shared/theme.hpp"

class QThread;

namespace xray::ui {
class StructurePage;
}

namespace xray::app {

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(ui::Theme theme, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    ui::Theme theme_;
    void loadDirectory(
        const QString& directory,
        ui::StructurePage* structure
    );

    QThread* sourceThread_ = nullptr;
    std::stop_source sourceStop_;
};

} // namespace xray::app
