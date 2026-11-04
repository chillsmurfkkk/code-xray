// ExportPage.cpp — D-UI4 «Експорт»
#include "ui/review/ExportPage.hpp"
#include "ui/review/review_fixtures.hpp"
#include "review/api.hpp"
#include "review/exporter.hpp"

#include <nlohmann/json.hpp>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressBar>
#include <QThread>

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

    cancelButton_ = new QPushButton(QString::fromUtf8("Скасувати"));
    cancelButton_->setObjectName("exportCancelButton");
    cancelButton_->setMinimumHeight(32);
    cancelButton_->setProperty("role", "secondaryAction");
    cancelButton_->setVisible(false);
    actionsLayout->addWidget(cancelButton_);

    mainLayout->addLayout(actionsLayout);

    // --- Прогрес експорту ---
    progressBar_ = new QProgressBar;
    progressBar_->setObjectName("exportProgressBar");
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setVisible(false);
    mainLayout->addWidget(progressBar_);

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
    connect(cancelButton_, &QPushButton::clicked, this, &ExportPage::cancel);

    // Демо-дані
    setReport(makeDemoComparisonReport(), /*isDemo=*/true);
}

ExportPage::~ExportPage()
{
    cancel();
    if (workerThread_) {
        workerThread_->wait();
    }
}

void ExportPage::cancel()
{
    if (isExporting_) {
        stopSource_.request_stop();
        statusLabel_->setText(QString::fromUtf8("Скасування експорту…"));
    }
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
        if (isVisible()) {
            QMessageBox::warning(this, QString::fromUtf8("Попередній перегляд"),
                                 QString::fromUtf8("Немає звіту для попереднього перегляду."));
        }
        return;
    }

    auto format = static_cast<ExportFormat>(formatCombo_->currentData().toInt());
    std::string previewContent = renderReport(currentReport_, format, nullptr);
    previewArea_->setPlainText(QString::fromStdString(previewContent));

    const QString prefix = isDemo_ ? QString::fromUtf8("[Демо] ") : "";
    statusLabel_->setText(QString::fromUtf8("%1Попередній перегляд сформовано.").arg(prefix));
}

// D-H12
void ExportPage::onExportRequested()
{
    if (isExporting_) {
        return;
    }

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
                emit exportCancelled();
                return;
            }
        }
        overwriteConfirmed = true;
    }

    ExportOptions options;
    options.destination = std::filesystem::path(pathStr.toStdWString());
    options.format = static_cast<ExportFormat>(formatCombo_->currentData().toInt());
    options.overwriteConfirmed = overwriteConfirmed;

    if (workerThread_ && workerThread_->isRunning()) {
        workerThread_->wait();
    }

    isExporting_ = true;
    exportButton_->setEnabled(false);
    cancelButton_->setVisible(true);
    progressBar_->setValue(0);
    progressBar_->setVisible(true);
    statusLabel_->setText(QString::fromUtf8("Експорт звіту…"));

    stopSource_ = std::stop_source{};
    const auto token = stopSource_.get_token();
    const auto jobId = ++currentJobId_;

    workerThread_ = QThread::create([this, report = currentReport_, options, jobId, token]() {
        using ExportRes = Result<ExportReceipt>;
        auto result = std::make_shared<ExportRes>(Cancelled{});

        try {
            JobContext ctx;
            ctx.jobId = jobId;
            ctx.stopToken = token;
            ctx.onProgress = [this, jobId](const Progress& p) {
                QMetaObject::invokeMethod(this, [this, jobId, p]() {
                    if (jobId == currentJobId_ && isExporting_) {
                        int pct = (p.total.has_value() && *p.total > 0)
                            ? static_cast<int>((p.completed * 100) / *p.total)
                            : static_cast<int>(p.completed);
                        progressBar_->setValue(pct);
                        statusLabel_->setText(QString::fromUtf8("Експорт: %1 (%2%)")
                            .arg(QString::fromStdString(p.stage))
                            .arg(pct));
                    }
                }, Qt::QueuedConnection);
            };

            *result = exportReport(report, options, ctx);
        } catch (const std::exception& e) {
            *result = Error{ErrorCode::export_error, e.what()};
        } catch (...) {
            *result = Error{ErrorCode::export_error, "Невідома помилка під час експорту"};
        }

        QMetaObject::invokeMethod(this, [this, jobId, result]() {
            if (jobId != currentJobId_) {
                return;
            }

            isExporting_ = false;
            exportButton_->setEnabled(true);
            cancelButton_->setVisible(false);
            progressBar_->setVisible(false);

            if (stopSource_.stop_requested() || std::holds_alternative<Cancelled>(*result)) {
                statusLabel_->setText(QString::fromUtf8("Експорт скасовано."));
                emit exportCancelled();
                return;
            }

            if (std::holds_alternative<Error>(*result)) {
                const auto& err = std::get<Error>(*result);
                statusLabel_->setText(QString::fromUtf8("Помилка експорту: ") + QString::fromStdString(err.message));
                if (isVisible()) {
                    QMessageBox::critical(this, QString::fromUtf8("Помилка експорту"),
                                          QString::fromStdString(err.message));
                }
                emit exportFailed(QString::fromStdString(err.message));
            } else if (auto* receipt = std::get_if<ExportReceipt>(result.get())) {
                statusLabel_->setText(QString::fromUtf8("Успішно збережено: %1 (%2 байт)")
                    .arg(QString::fromStdString(receipt->path.string()))
                    .arg(static_cast<qlonglong>(receipt->bytesWritten)));
                emit exportCompleted(QString::fromStdString(receipt->path.string()), receipt->bytesWritten);
            }
        }, Qt::QueuedConnection);
    });

    workerThread_->start();
}

} // namespace xray::ui::review
