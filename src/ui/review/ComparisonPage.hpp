#pragma once
// ComparisonPage.hpp — D-UI1 «Порівняння»
// Таблиця змін, фільтри, кнопка порівняння.

#include "review/types.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace xray::ui::review {

class ComparisonPage : public QWidget {
    Q_OBJECT
public:
    explicit ComparisonPage(QWidget* parent = nullptr);

    /// Завантажити звіт для відображення.
    void setReport(const xray::review::ComparisonReport& report, bool isDemo = false,
                   const QString& baseOid = {}, const QString& targetOid = {});

    /// Отримати поточний звіт.
    const xray::review::ComparisonReport& currentReport() const { return currentReport_; }

    /// Чи є звіт демонстраційним.
    bool isDemo() const { return isDemo_; }

    const QString& baseOid() const { return baseOid_; }
    const QString& targetOid() const { return targetOid_; }

signals:
    /// D-H03: вибір рядка таблиці → відкрити картку зміни (передає оригінальний індекс сутності у звіті).
    void reviewItemSelected(int index);

    /// Запит на перебудову порівняння для конкретних комітів (MainWindow з'єднує з Coordinator).
    void compareRebuildRequested(const QString& baseOid, const QString& targetOid);

    /// Запит на порівняння.
    void compareRequested();

private slots:
    void onCompareRequested();          // D-H01
    void onReviewFilterChanged();       // D-H02
    void onTableRowSelected(int row, int column);

private:
    void populateTable();
    void updateSummary();

    static QString kindToString(xray::review::ReviewKind kind);
    static QString confidenceToString(xray::review::Confidence conf);

    struct FilteredRow {
        xray::review::ReviewItem item;
        int originalIndex = 0;
    };
    std::vector<FilteredRow> currentFilteredRows_;
    bool isDemo_ = true;

    QLabel*       demoBadge_        = nullptr;  // Індикатор демо-даних [P2]
    QLineEdit*    pathFilterEdit_   = nullptr;  // Поле фільтра шляху
    QComboBox*    kindFilterCombo_  = nullptr;  // Список виду зміни
    QCheckBox*    showUnknownCheck_ = nullptr;  // Прапорець невідомих результатів
    QPushButton*  compareButton_    = nullptr;  // Кнопка порівняти
    QTableWidget* reviewTable_      = nullptr;  // Таблиця змін
    QLabel*       summaryLabel_     = nullptr;

    QString baseOid_;
    QString targetOid_;

    xray::review::ComparisonReport currentReport_;
};

} // namespace xray::ui::review
