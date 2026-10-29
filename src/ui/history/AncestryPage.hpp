#pragma once

#include "history/types.hpp"
#include <QWidget>
#include <vector>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace xray::ui::history {

class AncestryPage : public QWidget {
    Q_OBJECT
public:
    explicit AncestryPage(QWidget* parent = nullptr);

    void setCommits(const std::vector<xray::history::CommitRecord>& commits);
    const std::vector<xray::history::CommitRecord>& commits() const { return rawCommits_; }

private slots:
    void onCheckAncestry();
    void onTraceLinearHistory();

private:
    void populateLinearTable(const std::vector<xray::history::CommitRecord>& chain);

    std::vector<xray::history::CommitRecord> rawCommits_;

    QLineEdit* ancestorOidEdit_ = nullptr;
    QLineEdit* descendantOidEdit_ = nullptr;
    QPushButton* checkAncestryBtn_ = nullptr;
    QLabel* ancestryResultLabel_ = nullptr;

    QLineEdit* linearStartEdit_ = nullptr;
    QPushButton* traceLinearBtn_ = nullptr;
    QLabel* linearStatusLabel_ = nullptr;
    QTableWidget* linearTable_ = nullptr;
};

} // namespace xray::ui::history
