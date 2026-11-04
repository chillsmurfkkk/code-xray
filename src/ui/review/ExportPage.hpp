#pragma once
// ExportPage.hpp — D-UI4 «Експорт»
// Вибір формату, шляху, попередній перегляд і збереження.

#include "review/types.hpp"
#include "common/job.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <stop_token>

class QThread;

namespace xray::ui::review {

class ExportPage : public QWidget {
    Q_OBJECT
public:
    explicit ExportPage(QWidget* parent = nullptr);
    ~ExportPage() override;

    /// Встановити поточний звіт для експорту.
    void setReport(const xray::review::ComparisonReport& report, bool isDemo = false);

    bool isDemo() const { return isDemo_; }
    bool hasReport() const { return hasReport_; }
    bool isExporting() const { return isExporting_; }
    bool isPreviewing() const { return isPreviewing_; }
    const xray::review::ComparisonReport& currentReport() const { return currentReport_; }

    void cancel();

signals:
    void exportCompleted(const QString& path, std::uintmax_t bytesWritten);
    void exportCancelled();
    void exportFailed(const QString& message);
    void previewCompleted();
    void previewCancelled();

private slots:
    void onExportFormatChanged();    // D-H10
    void onPreviewRequested();       // D-H11
    void onExportRequested();        // D-H12

private:
    QComboBox*    formatCombo_      = nullptr;  // Список формату
    QLineEdit*    pathEdit_         = nullptr;  // Поле вихідного шляху
    QCheckBox*    includeCodeCheck_ = nullptr;  // Прапорець фрагментів коду
    QPushButton*  previewButton_    = nullptr;  // Кнопка попереднього перегляду
    QPushButton*  exportButton_     = nullptr;  // Кнопка збереження
    QPushButton*  cancelButton_     = nullptr;  // Кнопка скасування
    QProgressBar* progressBar_      = nullptr;  // Індикатор поступу
    QTextEdit*    previewArea_      = nullptr;
    QLabel*       statusLabel_      = nullptr;
    QLabel*       demoBadge_        = nullptr;

    xray::review::ComparisonReport currentReport_;
    bool hasReport_ = false;
    bool isDemo_ = true;

    QThread* workerThread_ = nullptr;
    std::stop_source stopSource_;
    JobId currentJobId_ = 0;
    bool isExporting_ = false;
    bool isPreviewing_ = false;
};

} // namespace xray::ui::review
