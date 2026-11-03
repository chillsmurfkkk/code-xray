#pragma once
// ExportPage.hpp — D-UI4 «Експорт»
// Вибір формату, шляху, попередній перегляд і збереження.

#include "review/types.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace xray::ui::review {

class ExportPage : public QWidget {
    Q_OBJECT
public:
    explicit ExportPage(QWidget* parent = nullptr);

    /// Встановити поточний звіт для експорту.
    void setReport(const xray::review::ComparisonReport& report, bool isDemo = false);

    bool isDemo() const { return isDemo_; }
    bool hasReport() const { return hasReport_; }
    const xray::review::ComparisonReport& currentReport() const { return currentReport_; }

private slots:
    void onExportFormatChanged();    // D-H10
    void onPreviewRequested();       // D-H11
    void onExportRequested();        // D-H12

private:
    QComboBox*   formatCombo_      = nullptr;  // Список формату
    QLineEdit*   pathEdit_         = nullptr;  // Поле вихідного шляху
    QCheckBox*   includeCodeCheck_ = nullptr;  // Прапорець фрагментів коду
    QPushButton* previewButton_    = nullptr;  // Кнопка попереднього перегляду
    QPushButton* exportButton_     = nullptr;  // Кнопка збереження
    QTextEdit*   previewArea_      = nullptr;
    QLabel*      statusLabel_      = nullptr;
    QLabel*      demoBadge_        = nullptr;

    xray::review::ComparisonReport currentReport_;
    bool hasReport_ = false;
    bool isDemo_ = true;
};

} // namespace xray::ui::review
