#pragma once

#include "history/types.hpp"
#include <QWidget>
#include <vector>

class QLabel;
class QLineEdit;
class QComboBox;
class QPushButton;
class QTableWidget;

namespace xray::ui::history {

class CodeChurnPage : public QWidget {
    Q_OBJECT
public:
    explicit CodeChurnPage(QWidget* parent = nullptr);

    void setChanges(const std::vector<xray::history::ChangeRecord>& changes, bool isDemo = true);
    void setFileHistory(const QString& relativePath, const QString& commitOid,
                        const std::vector<xray::history::ChangeRecord>& fileChanges,
                        bool hasMore = false);
    void filterByFileAndCommit(const QString& relativePath, const QString& commitOid = QString());
    bool isFileHistoryMode() const { return isFileHistoryMode_; }
    bool hasMore() const { return hasMore_; }
    const std::vector<xray::history::ChangeRecord>& repositoryChanges() const { return repositoryChanges_; }
    const std::vector<xray::history::ChangeRecord>& changes() const { return rawChanges_; }
    const xray::history::CodeChurn& currentChurn() const { return currentChurn_; }
    bool isDemo() const { return isDemo_; }

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onFilterChanged();
    void onResetFilter();

private:
    void updateKpis(const std::vector<xray::history::ChangeRecord>& filtered);
    void populateTable(const std::vector<xray::history::ChangeRecord>& list);
    static QString statusToString(xray::history::ChangeStatus status);

    std::vector<xray::history::ChangeRecord> rawChanges_;
    std::vector<xray::history::ChangeRecord> repositoryChanges_;
    std::vector<xray::history::ChangeRecord> fileHistoryChanges_;
    std::vector<xray::history::ChangeRecord> displayedChanges_;
    xray::history::CodeChurn currentChurn_;
    bool isDemo_ = true;
    bool isFileHistoryMode_ = false;
    bool hasMore_ = false;
    QString exactPathFilter_;
    QString exactCommitFilter_;

    QLabel* demoBadge_ = nullptr;
    QLabel* addedLinesLabel_ = nullptr;
    QLabel* deletedLinesLabel_ = nullptr;
    QLabel* netLinesLabel_ = nullptr;
    QLabel* filesCountLabel_ = nullptr;
    QLabel* completenessBadge_ = nullptr;

    QComboBox* statusCombo_ = nullptr;
    QLineEdit* pathEdit_ = nullptr;
    QPushButton* recomputeBtn_ = nullptr;
    QPushButton* resetBtn_ = nullptr;
    QTableWidget* changesTable_ = nullptr;
};

} // namespace xray::ui::history
