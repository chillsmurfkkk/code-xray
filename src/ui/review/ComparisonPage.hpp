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
    void setReport(const xray::review::ComparisonReport& report);

signals:
    /// D-H03: вибір рядка таблиці → відкрити картку зміни.
    void reviewItemSelected(int index);

private slots:
    void onCompareRequested();          // D-H01
    void onReviewFilterChanged();       // D-H02
    void onTableRowSelected(int row, int column);

private:
    void populateTable(const std::vector<xray::review::ReviewItem>& items);
    void updateSummary(const std::vector<xray::review::ReviewItem>& items);

    static QString kindToString(xray::review::ReviewKind kind);
    static QString confidenceToString(xray::review::Confidence conf);

    QLineEdit*    pathFilterEdit_   = nullptr;  // Поле фільтра шляху
    QComboBox*    kindFilterCombo_  = nullptr;  // Список виду зміни
    QCheckBox*    showUnknownCheck_ = nullptr;  // Прапорець невідомих результатів
    QPushButton*  compareButton_    = nullptr;  // Кнопка порівняти
    QTableWidget* reviewTable_      = nullptr;  // Таблиця змін
    QLabel*       summaryLabel_     = nullptr;

    xray::review::ComparisonReport currentReport_;
};

} // namespace xray::ui::review
