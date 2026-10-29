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

    void setCommits(const std::vector<xray::history::CommitRecord>& commits);
    const std::vector<xray::history::CommitRecord>& filteredCommits() const { return filteredCommits_; }

private slots:
    void onApplyFilters();
    void onResetFilters();
    void onTableRowSelected(int row, int col);

private:
    void populateTable(const std::vector<xray::history::CommitRecord>& list);

    std::vector<xray::history::CommitRecord> rawCommits_;
    std::vector<xray::history::CommitRecord> filteredCommits_;

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
