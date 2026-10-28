#pragma once
#include "ui/analysis/session.hpp"
#include <QWidget>
class QTableWidget;
class QLineEdit;
class QLabel;
class QPushButton;
namespace xray::ui::analysis_pages {
class RulesPage final : public QWidget {
    Q_OBJECT
  public:
    explicit RulesPage(Session& session, QWidget* parent = nullptr);

  private:
    void onRulesApplied();
    bool readDraft();
    void loadProfile();
    Session& session_;
    QTableWidget* table_;
    std::array<QLineEdit*, 3> limits_;
    QLabel* error_;
    QPushButton* apply_;
};
} // namespace xray::ui::analysis_pages
