#pragma once
#include "analysis/types.hpp"
#include "ui/review/review_table_delegate.hpp"
#include <QHeaderView>
#include <QTableWidget>
#include <map>

namespace xray::ui::analysis_pages {
inline QString text(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}
inline QString validityName(code::Validity validity) {
    switch (validity) {
    case code::Validity::valid:
        return QStringLiteral("Доступне");
    case code::Validity::syntactic_only:
        return QStringLiteral("Лише синтаксично");
    case code::Validity::not_applicable:
        return QStringLiteral("Не застосовується");
    case code::Validity::unavailable:
        return QStringLiteral("Невідоме");
    }
    return {};
}
inline QString ruleName(int index) {
    return QStringList{QStringLiteral("Довга функція"), QStringLiteral("Глибока вкладеність"),
                       QStringLiteral("Багато розгалужень")}
        .value(index);
}
inline void prepareTable(QTableWidget* table, const QStringList& headers) {
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    xray::ui::review::setupReviewTable(table);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(32);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setStretchLastSection(true);
}
inline void fitColumns(QTableWidget* table) {
    table->resizeColumnsToContents();
    for (int column = 0; column < table->columnCount(); ++column)
        table->setColumnWidth(column, table->columnWidth(column) + 16);
}
inline std::map<std::string, const code::FunctionEntity*>
functionIndex(const xray::analysis::AnalysisSnapshot& snapshot) {
    std::map<std::string, const code::FunctionEntity*> result;
    if (snapshot.parsed)
        for (const auto& unit : snapshot.parsed->units)
            for (const auto& fn : unit.functions)
                if (fn)
                    result.emplace(fn->id, fn.get());
    return result;
}
} // namespace xray::ui::analysis_pages
