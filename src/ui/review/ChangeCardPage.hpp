#pragma once
// ChangeCardPage.hpp — D-UI3 «Картка зміни»
// Перегляд коду до/після, метрики, порушення.

#include "review/types.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace xray::ui::review {

class ChangeCardPage : public QWidget {
    Q_OBJECT
public:
    explicit ChangeCardPage(QWidget* parent = nullptr);

    /// Відобразити конкретний ReviewItem зі звіту.
    void showItem(const xray::review::ReviewItem& item,
                  const xray::review::ComparisonReport& report);

signals:
    /// D-H09: запит переходу до історії файла.
    void fileHistoryRequested(const QString& relativePath, const QString& commitOid);

private slots:
    void onMetricRowSelected(int row, int column);         // D-H07
    void onRelatedFindingSelected(int row);                // D-H08
    void onFileHistoryRequested();                          // D-H09

private:
    QLabel*       titleLabel_      = nullptr;
    QTextEdit*    beforeCodeEdit_  = nullptr;  // Перегляд коду «до»
    QTextEdit*    afterCodeEdit_   = nullptr;  // Перегляд коду «після»
    QTableWidget* metricsTable_    = nullptr;  // Таблиця значень метрик
    QListWidget*  findingsList_    = nullptr;  // Список пов'язаних порушень
    QPushButton*  historyButton_   = nullptr;  // Кнопка переходу до історії
    QLabel*       explanationLabel_ = nullptr;

    xray::review::ReviewItem currentItem_;
    QString currentPath_;
};

} // namespace xray::ui::review
