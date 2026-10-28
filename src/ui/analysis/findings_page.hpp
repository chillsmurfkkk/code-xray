#pragma once
#include "ui/analysis/session.hpp"
#include <QWidget>
class QTableWidget;
class QComboBox;
class QLineEdit;
class QPushButton;
namespace xray::ui {
class CodeView;
}
class QLabel;
namespace xray::ui::analysis_pages {
class FindingsPage final : public QWidget {
    Q_OBJECT
  public:
    explicit FindingsPage(Session& session, QWidget* parent = nullptr);
  signals:
    void codeActivated(std::shared_ptr<const xray::analysis::AnalysisSnapshot> snapshot, QString entityId);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void onFindingActivated();
    void refresh();
    Session& session_;
    QTableWidget* table_;
    QComboBox* rule_;
    QLineEdit *search_, *path_;
    QPushButton *navigate_, *reanalyze_;
    xray::ui::CodeView* code_ = nullptr;
    QLabel *explanation_, *count_;
    std::vector<xray::analysis::Finding> visible_;
};
} // namespace xray::ui::analysis_pages
