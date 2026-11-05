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
    cleanupWorkerThread();
}

void ExportPage::cleanupWorkerThread()
{
    if (workerThread_) {
        if (workerThread_->isRunning()) {
            stopSource_.request_stop();
            workerThread_->wait();
        }
        workerThread_ = nullptr;
    }
}

void ExportPage::cancel()
{
    if (isExporting_ || isPreviewing_) {
        stopSource_.request_stop();
        if (isExporting_) {
            statusLabel_->setText(QString::fromUtf8("Скасування експорту…"));
        } else if (isPreviewing_) {
            statusLabel_->setText(QString::fromUtf8("Скасування попереднього перегляду…"));
        }
    }
}

void ExportPage::setReport(const xray::review::ComparisonReport& report, bool isDemo)
{
    // [P2] Якщо виконується формування прев'ю для старого звіту, скасовуємо його
    // та інвалідуємо previewJobId_, щоб застарілий результат не потрапив на сторінку.
    if (isPreviewing_) {
        stopSource_.request_stop();
        isPreviewing_ = false;
        if (previewButton_) previewButton_->setEnabled(true);
        if (exportButton_) exportButton_->setEnabled(true);
        if (cancelButton_) cancelButton_->setVisible(false);
        if (progressBar_) progressBar_->setVisible(false);
    }
    ++previewJobId_;
    if (previewArea_) previewArea_->clear();

    currentReport_ = report;
    hasReport_ = true;
    isDemo_ = isDemo;
    if (demoBadge_) {
        demoBadge_->setVisible(isDemo_);
    }

    // [P2] Якщо експорт зараз триває (або завершується запис файла на диск), не скидаємо
    // його стан та статусний текст, щоб користувач отримав підтвердження про збереження.
    if (!isExporting_) {
        const QString prefix = isDemo_ ? QString::fromUtf8("[Демо] ") : "";
        statusLabel_->setText(
            QString::fromUtf8("%1Звіт завантажено: %2 елементів.")
                .arg(prefix)
                .arg(static_cast<qlonglong>(report.items.size())));
    }
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
    if (isExporting_ || isPreviewing_) {
        return;
    }

    if (!hasReport_) {
        statusLabel_->setText(QString::fromUtf8("Немає звіту для попереднього перегляду."));
        if (isVisible()) {
            QMessageBox::warning(this, QString::fromUtf8("Попередній перегляд"),
                                 QString::fromUtf8("Немає звіту для попереднього перегляду."));
        }
        return;
    }

    cleanupWorkerThread();

    isPreviewing_ = true;
    previewButton_->setEnabled(false);
    exportButton_->setEnabled(false);
    cancelButton_->setVisible(true);
    progressBar_->setValue(0);
    progressBar_->setVisible(true);
    statusLabel_->setText(QString::fromUtf8("Формування попереднього перегляду…"));

    stopSource_ = std::stop_source{};
    const auto token = stopSource_.get_token();
    const auto jobId = ++previewJobId_;
    const auto format = static_cast<ExportFormat>(formatCombo_->currentData().toInt());

    workerThread_ = QThread::create([this, report = currentReport_, format, jobId, token]() {
        std::string previewContent;
        bool wasCancelled = false;

        try {
            JobContext ctx;
            ctx.jobId = jobId;
            ctx.stopToken = token;
            ctx.onProgress = [this, jobId](const Progress& p) {
                QMetaObject::invokeMethod(this, [this, jobId, p]() {
                    if (jobId == previewJobId_ && isPreviewing_) {
                        int pct = (p.total.has_value() && *p.total > 0)
                            ? static_cast<int>((p.completed * 100) / *p.total)
                            : static_cast<int>(p.completed);
                        progressBar_->setValue(pct);
                        statusLabel_->setText(QString::fromUtf8("Прев'ю: %1 (%2%)")
                            .arg(QString::fromStdString(p.stage))
                            .arg(pct));
                    }
                }, Qt::QueuedConnection);
            };

            previewContent = renderReport(report, format, &ctx);
            if (ctx.isCancelled()) {
                wasCancelled = true;
            }
        } catch (...) {
            wasCancelled = true;
        }

        QMetaObject::invokeMethod(this, [this, jobId, previewContent = std::move(previewContent), wasCancelled]() {
            if (jobId != previewJobId_) {
                return;
            }

            isPreviewing_ = false;
            previewButton_->setEnabled(true);
            exportButton_->setEnabled(true);
            cancelButton_->setVisible(false);
            progressBar_->setVisible(false);

            if (wasCancelled || stopSource_.stop_requested()) {
                statusLabel_->setText(QString::fromUtf8("Попередній перегляд скасовано."));
                emit previewCancelled();
                return;
            }

            previewArea_->setPlainText(QString::fromStdString(previewContent));
            const QString prefix = isDemo_ ? QString::fromUtf8("[Демо] ") : "";
            statusLabel_->setText(QString::fromUtf8("%1Попередній перегляд сформовано.").arg(prefix));
            emit previewCompleted();
        }, Qt::QueuedConnection);
    });

    // [P3] Звільнення об'єкта QThread після завершення без витоків пам'яті
    connect(workerThread_, &QThread::finished, workerThread_, &QObject::deleteLater);
    connect(workerThread_, &QThread::finished, this, [this, thread = workerThread_]() {
        if (workerThread_ == thread) {
            workerThread_ = nullptr;
        }
    });

    workerThread_->start();
}

// D-H12
void ExportPage::onExportRequested()
{
    if (isExporting_ || isPreviewing_) {
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

    cleanupWorkerThread();

    isExporting_ = true;
    exportButton_->setEnabled(false);
    previewButton_->setEnabled(false);
    cancelButton_->setVisible(true);
    progressBar_->setValue(0);
    progressBar_->setVisible(true);
    statusLabel_->setText(QString::fromUtf8("Експорт звіту…"));

    stopSource_ = std::stop_source{};
    const auto token = stopSource_.get_token();
    const auto jobId = ++exportJobId_;

    workerThread_ = QThread::create([this, report = currentReport_, options, jobId, token]() {
        using ExportRes = Result<ExportReceipt>;
        auto result = std::make_shared<ExportRes>(Cancelled{});

        try {
            JobContext ctx;
            ctx.jobId = jobId;
            ctx.stopToken = token;
            ctx.onProgress = [this, jobId](const Progress& p) {
                QMetaObject::invokeMethod(this, [this, jobId, p]() {
                    if (jobId == exportJobId_ && isExporting_) {
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
            // [P2] Якщо файл уже успішно збережено на диск, ми обов'язково окремо обробляємо цей успішний результат
            // (виводимо статус та випромінюємо exportCompleted), навіть якщо звіт на сторінці змінився!
            if (auto* receipt = std::get_if<ExportReceipt>(result.get())) {
                isExporting_ = false;
                exportButton_->setEnabled(true);
                previewButton_->setEnabled(true);
                cancelButton_->setVisible(false);
                progressBar_->setVisible(false);

                statusLabel_->setText(QString::fromUtf8("Успішно збережено: %1 (%2 байт)")
                    .arg(QString::fromStdString(receipt->path.string()))
                    .arg(static_cast<qlonglong>(receipt->bytesWritten)));
                emit exportCompleted(QString::fromStdString(receipt->path.string()), receipt->bytesWritten);
                return;
            }

            if (jobId != exportJobId_) {
                return;
            }

            isExporting_ = false;
            exportButton_->setEnabled(true);
            previewButton_->setEnabled(true);
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
            }
        }, Qt::QueuedConnection);
    });

    // [P3] Звільнення об'єкта QThread після завершення без витоків пам'яті
    connect(workerThread_, &QThread::finished, workerThread_, &QObject::deleteLater);
    connect(workerThread_, &QThread::finished, this, [this, thread = workerThread_]() {
        if (workerThread_ == thread) {
            workerThread_ = nullptr;
        }
    });

    workerThread_->start();
}

} // namespace xray::ui::review
