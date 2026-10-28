#pragma once

#include <QMainWindow>
#include <stop_token>

#include "ui/shared/theme.hpp"

class QThread;
class QStackedWidget;

namespace xray::ui {
class StructurePage;
namespace review {
class ComparisonPage;
class TrendPage;
class ChangeCardPage;
class ExportPage;
}
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

    QStackedWidget* contentStack_ = nullptr;
    ui::StructurePage* structure_ = nullptr;
    ui::review::ComparisonPage* comparisonPage_ = nullptr;
    ui::review::TrendPage* trendPage_ = nullptr;
    ui::review::ChangeCardPage* changeCardPage_ = nullptr;
    ui::review::ExportPage* exportPage_ = nullptr;
};

} // namespace xray::app
