// ExportPage.cpp — D-UI4 «Експорт»
#include "ui/review/ExportPage.hpp"
#include "ui/review/review_fixtures.hpp"
#include "review/api.hpp"
#include "review/exporter.hpp"

#include <nlohmann/json.hpp>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>

namespace xray::ui::review {

using namespace xray::review;

ExportPage::ExportPage(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // --- Заголовок та індикатор демо ---
    auto* headerLayout = new QHBoxLayout;
    auto* titleLabel = new QLabel(QString::fromUtf8("Експорт звіту"));
    auto titleFont = titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 4);
    titleFont.setWeight(QFont::Bold);
    titleLabel->setFont(titleFont);
    headerLayout->addWidget(titleLabel);

    demoBadge_ = new QLabel(QString::fromUtf8("Демо-режим"));
    demoBadge_->setObjectName("exportDemoBadge");
    demoBadge_->setStyleSheet(
        "background: #d65d0e; color: #ebdbb2; font-weight: bold; "
        "border-radius: 8px; padding: 3px 8px; font-size: 11px;"
    );
    demoBadge_->setToolTip(QString::fromUtf8("Експортуються демонстраційні дані звіту."));
    headerLayout->addWidget(demoBadge_);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // --- Параметри ---
    auto* paramsLayout = new QHBoxLayout;

    paramsLayout->addWidget(new QLabel(QString::fromUtf8("Формат:")));
    formatCombo_ = new QComboBox;
    formatCombo_->setObjectName("exportFormatCombo");
    formatCombo_->setMinimumHeight(32);
    formatCombo_->setProperty("role", "inputField");
    formatCombo_->addItem("HTML", static_cast<int>(ExportFormat::html));
    formatCombo_->addItem("JSON", static_cast<int>(ExportFormat::json));
    paramsLayout->addWidget(formatCombo_);

    pathEdit_ = new QLineEdit;
    pathEdit_->setObjectName("exportPathEdit");
    pathEdit_->setMinimumHeight(32);
    pathEdit_->setProperty("role", "inputField");
    pathEdit_->setPlaceholderText(QString::fromUtf8("Шлях для збереження…"));
    paramsLayout->addWidget(pathEdit_, /*stretch=*/1);

    auto* browseButton = new QPushButton("…");
    browseButton->setObjectName("exportBrowseButton");
    browseButton->setFixedWidth(40);
    browseButton->setMinimumHeight(32);
    browseButton->setProperty("role", "primaryAction");
    connect(browseButton, &QPushButton::clicked, this, [this]() {
        auto suffix = formatCombo_->currentData().toInt() == static_cast<int>(ExportFormat::html)
            ? "HTML (*.html)" : "JSON (*.json)";
        auto path = QFileDialog::getSaveFileName(this, QString::fromUtf8("Зберегти звіт"), {}, suffix);
        if (!path.isEmpty()) pathEdit_->setText(path);
    });
    paramsLayout->addWidget(browseButton);

    mainLayout->addLayout(paramsLayout);

    // --- Прапорець та кнопки ---
    auto* actionsLayout = new QHBoxLayout;

    includeCodeCheck_ = new QCheckBox(QString::fromUtf8("Включити фрагменти коду"));
    includeCodeCheck_->setObjectName("exportIncludeCodeCheck");
    includeCodeCheck_->setChecked(true);
    actionsLayout->addWidget(includeCodeCheck_);

    actionsLayout->addStretch();

    previewButton_ = new QPushButton(QString::fromUtf8("Попередній перегляд"));
    previewButton_->setObjectName("exportPreviewButton");
    previewButton_->setMinimumHeight(32);
    previewButton_->setProperty("role", "primaryAction");
    actionsLayout->addWidget(previewButton_);

    exportButton_ = new QPushButton(QString::fromUtf8("Зберегти"));
    exportButton_->setObjectName("exportSaveButton");
    exportButton_->setMinimumHeight(32);
    exportButton_->setProperty("role", "primaryAction");
    actionsLayout->addWidget(exportButton_);

    mainLayout->addLayout(actionsLayout);

    // --- Статус ---
    statusLabel_ = new QLabel;
    statusLabel_->setObjectName("exportStatusLabel");
    mainLayout->addWidget(statusLabel_);

    // --- Область попереднього перегляду ---
    previewArea_ = new QTextEdit;
    previewArea_->setObjectName("exportPreviewArea");
    previewArea_->setReadOnly(true);
    previewArea_->setFont(QFont("Consolas", 9));
    previewArea_->setPlaceholderText(
        QString::fromUtf8("Натисніть «Попередній перегляд» для генерації."));
    mainLayout->addWidget(previewArea_, /*stretch=*/1);

    // --- Сигнали ---
    connect(formatCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExportPage::onExportFormatChanged);
    connect(previewButton_, &QPushButton::clicked, this, &ExportPage::onPreviewRequested);
    connect(exportButton_, &QPushButton::clicked, this, &ExportPage::onExportRequested);

    // Демо-дані
    setReport(makeDemoComparisonReport(), /*isDemo=*/true);
}

void ExportPage::setReport(const xray::review::ComparisonReport& report, bool isDemo)
{
    currentReport_ = report;
    hasReport_ = true;
    isDemo_ = isDemo;
    if (demoBadge_) {
        demoBadge_->setVisible(isDemo_);
    }
    const QString prefix = isDemo_ ? QString::fromUtf8("[Демо] ") : "";
    statusLabel_->setText(
        QString::fromUtf8("%1Звіт завантажено: %2 елементів.")
            .arg(prefix)
            .arg(static_cast<qlonglong>(report.items.size())));
}

// D-H10
void ExportPage::onExportFormatChanged()
{
    auto fmt = formatCombo_->currentData().toInt();
    if (fmt == static_cast<int>(ExportFormat::html)) {
        statusLabel_->setText(QString::fromUtf8("Обрано HTML — звіт зі стилями та таблицями."));
        // Оновити розширення у шляху
        auto path = pathEdit_->text();
        if (path.endsWith(".json", Qt::CaseInsensitive)) {
            path.replace(path.size() - 5, 5, ".html");
            pathEdit_->setText(path);
        }
    } else {
        statusLabel_->setText(QString::fromUtf8("Обрано JSON — машинно-читабельний формат."));
        auto path = pathEdit_->text();
        if (path.endsWith(".html", Qt::CaseInsensitive)) {
            path.replace(path.size() - 5, 5, ".json");
            pathEdit_->setText(path);
        }
    }
}

// D-H11
void ExportPage::onPreviewRequested()
{
    if (!hasReport_) {
        statusLabel_->setText(QString::fromUtf8("Немає звіту для попереднього перегляду."));
        return;
    }

    auto fmt = formatCombo_->currentData().toInt();
    if (fmt == static_cast<int>(ExportFormat::json)) {
        nlohmann::json root;
        root["schemaVersion"] = "comparison-v1";
        root["baseId"] = currentReport_.baseId;
        root["targetId"] = currentReport_.targetId;
        root["analysisFingerprint"] = currentReport_.analysisFingerprint;
        root["metricSchemaVersion"] = currentReport_.metricSchemaVersion;
        root["summary"] = summarizeComparison(currentReport_.items);

        nlohmann::json items = nlohmann::json::array();
        for (const auto& it : currentReport_.items) {
            nlohmann::json j;
            j["entityKey"] = it.entityKey;
            switch (it.kind) {
            case ReviewKind::added:     j["kind"] = "added"; break;
            case ReviewKind::removed:   j["kind"] = "removed"; break;
            case ReviewKind::changed:   j["kind"] = "changed"; break;
            case ReviewKind::unchanged: j["kind"] = "unchanged"; break;
            case ReviewKind::unmatched: j["kind"] = "unmatched"; break;
            }
            j["explanation"] = it.explanation;
            items.push_back(j);
        }
        root["items"] = items;
        previewArea_->setPlainText(QString::fromStdString(root.dump(2)));
    } else {
        auto model = buildReportModel(currentReport_);
        QString preview;
        preview += "<!DOCTYPE html>\n<html>\n<head>\n";
        preview += "  <meta charset=\"UTF-8\">\n";
        preview += "  <title>Code Xray Comparison Report</title>\n";
        preview += "</head>\n<body>\n";
        preview += "  <h1>Code Xray Comparison Report</h1>\n";
        preview += "  <p><strong>Base:</strong> " + QString::fromStdString(model.baseId) + "</p>\n";
        preview += "  <p><strong>Target:</strong> " + QString::fromStdString(model.targetId) + "</p>\n";
        preview += "  <p><strong>Summary:</strong> " + QString::fromStdString(model.summary) + "</p>\n";
        preview += "  <p><strong>Total Items:</strong> " + QString::number(model.totalItems) + "</p>\n";
        preview += "  <table border=\"1\" cellpadding=\"6\" cellspacing=\"0\">\n";
        preview += "    <tr><th>Kind</th><th>Entity</th><th>Path</th><th>Metrics</th><th>Explanation</th></tr>\n";
        for (const auto& r : model.rows) {
            preview += "    <tr>";
            preview += "<td>" + QString::fromStdString(r.kind) + "</td>";
            preview += "<td>" + QString::fromStdString(r.entityKey) + "</td>";
            preview += "<td>" + QString::fromStdString(r.path) + "</td>";
            preview += "<td>" + QString::fromStdString(r.metricsSummary) + "</td>";
            preview += "<td>" + QString::fromStdString(r.explanation) + "</td>";
            preview += "</tr>\n";
        }
        preview += "  </table>\n</body>\n</html>";
        previewArea_->setPlainText(preview);
    }

    const QString prefix = isDemo_ ? QString::fromUtf8("[Демо] ") : "";
    statusLabel_->setText(QString::fromUtf8("%1Попередній перегляд сформовано.").arg(prefix));
}

// D-H12
void ExportPage::onExportRequested()
{
    if (!hasReport_) {
        statusLabel_->setText(QString::fromUtf8("Немає звіту для експорту."));
        if (isVisible()) {
            QMessageBox::warning(this, QString::fromUtf8("Експорт"),
                                 QString::fromUtf8("Немає звіту для експорту."));
        }
        return;
    }

    const auto pathStr = pathEdit_->text().trimmed();
    if (pathStr.isEmpty()) {
        statusLabel_->setText(QString::fromUtf8("Вкажіть шлях для збереження."));
        if (isVisible()) {
            QMessageBox::warning(this, QString::fromUtf8("Експорт"),
                                 QString::fromUtf8("Вкажіть шлях для збереження."));
        }
        return;
    }

    bool overwriteConfirmed = false;
    if (QFile::exists(pathStr)) {
        if (isVisible()) {
            auto answer = QMessageBox::question(this, QString::fromUtf8("Перезаписати?"),
                QString::fromUtf8("Файл '%1' вже існує. Перезаписати?").arg(pathStr),
                QMessageBox::Yes | QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                statusLabel_->setText(QString::fromUtf8("Експорт скасовано."));
                return;
            }
        }
        overwriteConfirmed = true;
    }

    ExportOptions options;
    options.destination = std::filesystem::path(pathStr.toStdWString());
    options.format = static_cast<ExportFormat>(formatCombo_->currentData().toInt());
    options.overwriteConfirmed = overwriteConfirmed;

    JobContext ctx;
    auto result = exportReport(currentReport_, options, ctx);

    if (std::holds_alternative<Cancelled>(result)) {
        statusLabel_->setText(QString::fromUtf8("Експорт скасовано."));
    } else if (std::holds_alternative<Error>(result)) {
        const auto& err = std::get<Error>(result);
        statusLabel_->setText(QString::fromUtf8("Помилка експорту: ") + QString::fromStdString(err.message));
        if (isVisible()) {
            QMessageBox::critical(this, QString::fromUtf8("Помилка експорту"),
                                  QString::fromStdString(err.message));
        }
    } else if (auto* receipt = std::get_if<ExportReceipt>(&result)) {
        statusLabel_->setText(QString::fromUtf8("Успішно збережено: %1 (%2 байт)")
            .arg(QString::fromStdString(receipt->path.string()))
            .arg(receipt->bytesWritten));
    }
}

} // namespace xray::ui::review
