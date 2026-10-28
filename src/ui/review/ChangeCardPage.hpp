#pragma once
// ChangeCardPage.hpp — D-UI3 «Картка зміни»
// Перегляд коду до/після (CodeView з підсвічуванням Gruvbox), метрики, порушення.

#include "review/types.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace xray::ui {
class CodeView;
}

namespace xray::ui::review {

class ChangeCardPage : public QWidget {
    Q_OBJECT
public:
    explicit ChangeCardPage(QWidget* parent = nullptr);

    /// Відобразити конкретний ReviewItem зі звіту.
    void showItem(const xray::review::ReviewItem& item,
                  const xray::review::ComparisonReport& report);

signals:
    /// Повернення до таблиці порівняння
    void backToComparisonRequested();

    /// D-H09: запит переходу до історії файла.
    void fileHistoryRequested(const QString& relativePath, const QString& commitOid);

private slots:
    void onMetricRowSelected(int row, int column);         // D-H07
    void onRelatedFindingSelected(int row);                // D-H08
    void onFileHistoryRequested();                          // D-H09

private:
    QPushButton*  backButton_      = nullptr;
    QLabel*       titleLabel_      = nullptr;
    xray::ui::CodeView* beforeCodeView_ = nullptr;  // Перегляд коду «до» (з номерами рядків та синтаксисом)
    xray::ui::CodeView* afterCodeView_  = nullptr;  // Перегляд коду «після» (з номерами рядків та синтаксисом)
    QTableWidget* metricsTable_    = nullptr;       // Таблиця значень метрик
    QListWidget*  findingsList_    = nullptr;       // Список пов'язаних порушень
    QPushButton*  historyButton_   = nullptr;       // Кнопка переходу до історії
    QLabel*       explanationLabel_ = nullptr;

    xray::review::ReviewItem currentItem_;
    QString currentPath_;
};

} // namespace xray::ui::review
