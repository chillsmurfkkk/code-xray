#include "diagnostics_page.hpp"
#include "ui/shared/theme.hpp"

#include <QAbstractItemView>
#include <QFont>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTableWidget>
#include <QVBoxLayout>

#include <utility>

namespace xray::ui {

DiagnosticsPage::DiagnosticsPage(QWidget* parent)
    : QWidget(parent) {
    setObjectName("diagnosticsPage");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel(QString::fromUtf8("Діагностика"), this);
    auto titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 7);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* description = new QLabel(
        QString::fromUtf8("Проблеми збирання файлів і розбору коду."),
        this
    );
    description->setObjectName("sourceContext");
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* panel = new QFrame(this);
    panel->setObjectName("workPanel");
    layout->addWidget(panel, 1);

    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(16, 16, 16, 16);
    panelLayout->setSpacing(12);

    status_ = new QLabel(
        QString::fromUtf8("Відкрийте джерело, щоб побачити діагностику."),
        panel
    );
    status_->setObjectName("diagnosticsStatus");
    status_->setWordWrap(true);
    panelLayout->addWidget(status_);

    auto* filters = new QHBoxLayout;
    pathFilter_ = new QLineEdit(panel);
    pathFilter_->setObjectName("diagnosticsPathFilter");
    pathFilter_->setProperty("role", "inputField");
    pathFilter_->setMinimumHeight(36);
    pathFilter_->setPlaceholderText(QString::fromUtf8("Фільтр за шляхом файлу"));
    pathFilter_->setAccessibleName(QString::fromUtf8("Фільтр діагностики за шляхом"));
    filters->addWidget(pathFilter_, 1);
    kindFilter_ = new QComboBox(panel);
    kindFilter_->setObjectName("diagnosticsKindFilter");
    kindFilter_->setProperty("role", "inputField");
    kindFilter_->setAccessibleName(QString::fromUtf8("Тип діагностики"));
    kindFilter_->addItem(QString::fromUtf8("Усі повідомлення"), 0);
    kindFilter_->addItem(QString::fromUtf8("Збір файлів"), 1);
    kindFilter_->addItem(QString::fromUtf8("Розбір коду"), 2);
    filters->addWidget(kindFilter_);
    panelLayout->addLayout(filters);

    table_ = new QTableWidget(panel);
    table_->setObjectName("diagnosticsTable");
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({
        QString::fromUtf8("Файл"),
        QString::fromUtf8("Рядок"),
        QString::fromUtf8("Повідомлення")
    });
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setShowGrid(false);
    table_->setAccessibleName(QString::fromUtf8("Повідомлення діагностики"));
    styleWorkspaceScrollBar(table_->verticalScrollBar());
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    panelLayout->addWidget(table_, 1);

    auto* actions = new QHBoxLayout;
    reparse_ = new QPushButton(QString::fromUtf8("Сканувати знову"), panel);
    reparse_->setObjectName("diagnosticsReparse");
    reparse_->setToolTip(QString::fromUtf8("Сканувати поточне джерело з поточними правилами відбору"));
    open_ = new QPushButton(QString::fromUtf8("Перейти до коду"), panel);
    open_->setObjectName("diagnosticsOpenCode");
    for (auto* button : {reparse_, open_}) {
        button->setProperty("role", "primaryAction");
        button->setCursor(Qt::PointingHandCursor);
        actions->addWidget(button);
    }
    actions->addStretch();
    panelLayout->addLayout(actions);
    connect(pathFilter_, &QLineEdit::textChanged, this, &DiagnosticsPage::applyFilters);
    connect(kindFilter_, &QComboBox::currentIndexChanged, this, &DiagnosticsPage::applyFilters);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &DiagnosticsPage::updateActions);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int, int) { openSelected(); });
    connect(open_, &QPushButton::clicked, this, &DiagnosticsPage::openSelected);
    connect(reparse_, &QPushButton::clicked, this, &DiagnosticsPage::reparseRequested);
    updateActions();
}

void DiagnosticsPage::setSnapshot(
    std::shared_ptr<const code::ParsedSnapshot> snapshot,
    bool demonstration
) {
    snapshot_ = std::move(snapshot);
    demonstration_ = demonstration;
    table_->setRowCount(0);

    if (!snapshot_) {
        status_->setText(
            QString::fromUtf8("Відкрийте джерело, щоб побачити діагностику.")
        );
        updateActions();
        return;
    }

    const auto appendRow = [this](
        const QString& path,
        const QString& line,
        const QString& message,
        int kind,
        int lineNumber
    ) {
        const int row = table_->rowCount();
        table_->insertRow(row);
        auto* pathItem = new QTableWidgetItem(path);
        pathItem->setData(Qt::UserRole, kind);
        pathItem->setData(Qt::UserRole + 1, lineNumber);
        table_->setItem(row, 0, pathItem);
        table_->setItem(row, 1, new QTableWidgetItem(line));
        table_->setItem(row, 2, new QTableWidgetItem(message));
    };

    // Повідомлення збору файлів не мають діапазону рядків.
    if (snapshot_->source) {
        for (const auto& error : snapshot_->source->coverage.diagnostics) {
            appendRow(
                error.path ? QString::fromStdString(*error.path) : QStringLiteral("—"),
                QStringLiteral("—"),
                QString::fromStdString(error.message), 1, 0
            );
        }
    }

    // Діагностика розбору може містити номер рядка.
    for (const auto& unit : snapshot_->units) {
        for (const auto& diagnostic : unit.diagnostics) {
            appendRow(
                QString::fromStdString(diagnostic.relativePath),
                diagnostic.range
                    ? QString::number(diagnostic.range->startLine)
                    : QStringLiteral("—"),
                QString::fromStdString(diagnostic.error.message), 2,
                diagnostic.range ? static_cast<int>(diagnostic.range->startLine) : 0
            );
        }
    }

    applyFilters();
}

void DiagnosticsPage::applyFilters() {
    int visible = 0;
    const auto query = pathFilter_->text().trimmed();
    const int kind = kindFilter_->currentData().toInt();
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* item = table_->item(row, 0);
        const bool match = item && item->text().contains(query, Qt::CaseInsensitive)
            && (kind == 0 || item->data(Qt::UserRole).toInt() == kind);
        table_->setRowHidden(row, !match);
        if (match) ++visible;
    }
    if (!snapshot_) {
        status_->setText(QString::fromUtf8("Відкрийте джерело, щоб побачити діагностику."));
    } else {
        const QString prefix = demonstration_ ? QString::fromUtf8("Демонстраційний знімок · ") : QString();
        status_->setText(prefix + (table_->rowCount() == 0
            ? QString::fromUtf8("Повідомлень немає.")
            : visible == 0 ? QString::fromUtf8("За фільтрами повідомлень немає · усього: %1").arg(table_->rowCount())
            : QString::fromUtf8("Показано: %1 з %2").arg(visible).arg(table_->rowCount())));
    }
    if (table_->currentRow() >= 0 && table_->isRowHidden(table_->currentRow())) table_->clearSelection();
    updateActions();
}

void DiagnosticsPage::setLoading(bool loading) {
    loading_ = loading;
    updateActions();
}

void DiagnosticsPage::updateActions() {
    reparse_->setEnabled(!loading_);
    const int row = table_->currentRow();
    auto* item = row >= 0 && !table_->isRowHidden(row) && table_->selectionModel()->hasSelection()
        ? table_->item(row, 0) : nullptr;
    bool available = false;
    if (snapshot_ && item) {
        for (const auto& unit : snapshot_->units) {
            if (QString::fromStdString(unit.relativePath) == item->text() && unit.root && unit.root->sourceBytes) {
                available = true;
                break;
            }
        }
    }
    open_->setEnabled(!loading_ && available);
    open_->setToolTip(available ? QString() : QString::fromUtf8("Код вибраного файлу недоступний у знімку"));
}

void DiagnosticsPage::openSelected() {
    if (!open_->isEnabled()) return;
    auto* item = table_->item(table_->currentRow(), 0);
    emit openCodeRequested(item->text(), item->data(Qt::UserRole + 1).toInt());
}

} // namespace xray::ui
