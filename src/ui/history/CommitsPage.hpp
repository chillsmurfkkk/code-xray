#pragma once

#include "history/types.hpp"
#include <QWidget>
#include <vector>

class QLabel;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QPushButton;
class QTableWidget;

namespace xray::ui::history {

class CommitsPage : public QWidget {
    Q_OBJECT
public:
    explicit CommitsPage(QWidget* parent = nullptr);

    void setCommits(const std::vector<xray::history::CommitRecord>& commits, bool isDemo = true);
    const std::vector<xray::history::CommitRecord>& commits() const { return rawCommits_; }
    bool isDemo() const { return isDemo_; }

signals:
    void commitSelected(const QString& oid);

private slots:
    void onSelectionChanged();
    void onSearchChanged(const QString& text);
    void onSortToggled();
    void onTableRowSelected(int row, int col);
    void onRefreshClicked();

private:
    void populateTable(const std::vector<xray::history::CommitRecord>& list);
    void updateDetails(int rowIndex);

    std::vector<xray::history::CommitRecord> rawCommits_;
    std::vector<xray::history::CommitRecord> displayedCommits_;
    bool sortAscending_ = false;
    bool isDemo_ = true;

    QLabel* demoBadge_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QComboBox* selectionCombo_ = nullptr;
    QSpinBox* limitSpin_ = nullptr;
    QPushButton* refreshBtn_ = nullptr;
    QPushButton* sortBtn_ = nullptr;
    QLabel* countLabel_ = nullptr;
    QTableWidget* commitsTable_ = nullptr;
    QLabel* detailsLabel_ = nullptr;
};

} // namespace xray::ui::history
