#pragma once

#include "history/types.hpp"
#include "common/job.hpp"
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

    QString baseCommitOid() const { return baseOid_; }
    QString targetCommitOid() const { return targetOid_; }
    void setBaseCommitOid(const QString& oid);
    void setTargetCommitOid(const QString& oid);

    void setBusy(bool busy);
    void setProgress(const xray::Progress& progress);

signals:
    void commitSelected(const QString& oid);
    void compareRequested(const QString& baseOid, const QString& targetOid);
    void trendRequested(const QString& tipOid);
    void commitSnapshotRequested(const QString& oid);
    void openRepositoryRequested();
    void cancelRequested();

private slots:
    void onSelectionChanged();
    void onSearchChanged(const QString& text);
    void onSortToggled();
    void onTableRowSelected(int row, int col);
    void onRefreshClicked();
    void onSetBaseClicked();
    void onSetTargetClicked();
    void onCompareClicked();
    void onTrendClicked();
    void onOpenSnapshotClicked();

private:
    void populateTable(const std::vector<xray::history::CommitRecord>& list);
    void updateDetails(int rowIndex);
    void updateSelectionLabels();
    QString currentSelectedOid() const;

    std::vector<xray::history::CommitRecord> rawCommits_;
    std::vector<xray::history::CommitRecord> displayedCommits_;
    bool sortAscending_ = false;
    bool isDemo_ = true;

    QString baseOid_;
    QString targetOid_;

    QLabel* demoBadge_ = nullptr;
    QPushButton* openRepoBtn_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QComboBox* selectionCombo_ = nullptr;
    QSpinBox* limitSpin_ = nullptr;
    QPushButton* refreshBtn_ = nullptr;
    QPushButton* sortBtn_ = nullptr;
    QLabel* countLabel_ = nullptr;
    QTableWidget* commitsTable_ = nullptr;
    QLabel* detailsLabel_ = nullptr;

    // Two-commit & review actions
    QLabel* baseLabel_ = nullptr;
    QPushButton* setBaseBtn_ = nullptr;
    QLabel* targetLabel_ = nullptr;
    QPushButton* setTargetBtn_ = nullptr;
    QPushButton* compareBtn_ = nullptr;
    QPushButton* trendBtn_ = nullptr;
    QPushButton* openSnapshotBtn_ = nullptr;
    QPushButton* cancelBtn_ = nullptr;
    QLabel* progressLabel_ = nullptr;
};

} // namespace xray::ui::history
