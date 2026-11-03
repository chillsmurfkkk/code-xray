#pragma once
#include "ui/analysis/session.hpp"
#include <QWidget>
class QListWidget;
namespace xray::ui::analysis_pages {
class ProfilesPage final : public QWidget {
    Q_OBJECT
  public:
    explicit ProfilesPage(Session& session, QWidget* parent = nullptr);

  private:
    void onProfileCreated();
    void onProfileDuplicated();
    void onProfileImported();
    void onProfileSaved();
    void refresh();
    Session& session_;
    QListWidget* list_;
};
} // namespace xray::ui::analysis_pages
