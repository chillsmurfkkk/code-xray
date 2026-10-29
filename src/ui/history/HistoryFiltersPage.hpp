#pragma once

#include "history/types.hpp"
#include <QWidget>
#include <vector>

class QLabel;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QPushButton;
class QTableWidget;

namespace xray::ui::history {

class HistoryFiltersPage : public QWidget {
    Q_OBJECT
public:
    explicit HistoryFiltersPage(QWidget* parent = nullptr);

    void setCommits(const std::vector<xray::history::CommitRecord>& commits, bool isDemo = true);
    const std::vector<xray::history::CommitRecord>& filteredCommits() const { return filteredCommits_; }
    bool isDemo() const { return isDemo_; }

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onApplyFilters();
    void onResetFilters();
    void onTableRowSelected(int row, int col);

private:
    enum class StatusKind { Neutral, Success, Error };

    void populateTable(const std::vector<xray::history::CommitRecord>& list);
    void updateBadge(int activeCount);
    void refreshStatusStyle();

    std::vector<xray::history::CommitRecord> rawCommits_;
    std::vector<xray::history::CommitRecord> filteredCommits_;
    bool isDemo_ = true;
    StatusKind statusKind_ = StatusKind::Neutral;
    int activeFilterCount_ = 0;

    QLabel* demoBadge_ = nullptr;
    QLineEdit* rangeStartEdit_ = nullptr;
    QLineEdit* rangeEndEdit_ = nullptr;
    QLineEdit* authorEdit_ = nullptr;
    QCheckBox* authorCaseCheck_ = nullptr;
    QLineEdit* keywordEdit_ = nullptr;
    QCheckBox* keywordCaseCheck_ = nullptr;
    QSpinBox* minLengthSpin_ = nullptr;
    QCheckBox* onlyMergesCheck_ = nullptr;
    QSpinBox* minParentsSpin_ = nullptr;
    QPushButton* applyBtn_ = nullptr;
    QPushButton* resetBtn_ = nullptr;

    QLabel* statusLabel_ = nullptr;
    QLabel* activeFiltersBadge_ = nullptr;
    QTableWidget* resultsTable_ = nullptr;
};

} // namespace xray::ui::history
