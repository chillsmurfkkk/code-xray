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

    void setCommits(const std::vector<xray::history::CommitRecord>& commits, bool isDemo = true);
    const std::vector<xray::history::CommitRecord>& commits() const { return rawCommits_; }
    bool isDemo() const { return isDemo_; }

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onCheckAncestry();
    void onTraceLinearHistory();

private:
    enum class AncestryStatusKind { Neutral, Confirmed, Rejected, Error };
    enum class LinearStatusKind { Neutral, Success, Error };

    void updateAncestryResultStyle();
    void updateLinearStatusStyle();
    void populateLinearTable(const std::vector<xray::history::CommitRecord>& chain);

    std::vector<xray::history::CommitRecord> rawCommits_;
    bool isDemo_ = true;
    AncestryStatusKind ancestryStatusKind_ = AncestryStatusKind::Neutral;
    LinearStatusKind linearStatusKind_ = LinearStatusKind::Neutral;

    QLabel* demoBadge_ = nullptr;
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
