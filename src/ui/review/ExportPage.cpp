// ExportPage.cpp — D-UI4 «Експорт»
#include "ui/review/ExportPage.hpp"
#include "ui/review/review_fixtures.hpp"

#include <QFileDialog>
#include <QMessageBox>

namespace xray::ui::review {

using namespace xray::review;

ExportPage::ExportPage(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* titleLabel = new QLabel(QString::fromUtf8("Експорт звіту"));
    auto titleFont = titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 4);
    titleFont.setWeight(QFont::Bold);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // --- Параметри ---
    auto* paramsLayout = new QHBoxLayout;

    paramsLayout->addWidget(new QLabel(QString::fromUtf8("Формат:")));
    formatCombo_ = new QComboBox;
    formatCombo_->setMinimumHeight(32);
    formatCombo_->setProperty("role", "inputField");
    formatCombo_->addItem("HTML", static_cast<int>(ExportFormat::html));
    formatCombo_->addItem("JSON", static_cast<int>(ExportFormat::json));
    paramsLayout->addWidget(formatCombo_);

    pathEdit_ = new QLineEdit;
    pathEdit_->setMinimumHeight(32);
    pathEdit_->setProperty("role", "inputField");
    pathEdit_->setPlaceholderText(QString::fromUtf8("Шлях для збереження…"));
    paramsLayout->addWidget(pathEdit_, /*stretch=*/1);

    auto* browseButton = new QPushButton("…");
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
    includeCodeCheck_->setChecked(true);
    actionsLayout->addWidget(includeCodeCheck_);

    actionsLayout->addStretch();

    previewButton_ = new QPushButton(QString::fromUtf8("Попередній перегляд"));
    previewButton_->setMinimumHeight(32);
    previewButton_->setProperty("role", "primaryAction");
    actionsLayout->addWidget(previewButton_);

    exportButton_ = new QPushButton(QString::fromUtf8("Зберегти"));
    exportButton_->setMinimumHeight(32);
    exportButton_->setProperty("role", "primaryAction");
    actionsLayout->addWidget(exportButton_);

    mainLayout->addLayout(actionsLayout);

    // --- Статус ---
    statusLabel_ = new QLabel;
    statusLabel_->setObjectName("statusPill");
    mainLayout->addWidget(statusLabel_);

    // --- Область попереднього перегляду ---
    previewArea_ = new QTextEdit;
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
    setReport(makeDemoComparisonReport());
}

void ExportPage::setReport(const xray::review::ComparisonReport& report)
{
    currentReport_ = report;
    hasReport_ = true;
    statusLabel_->setText(
        QString::fromUtf8("Звіт завантажено: %1 елементів.")
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

    // DEMO: генеруємо простий текстовий перегляд
    QString preview;
    auto fmt = formatCombo_->currentData().toInt();

    if (fmt == static_cast<int>(ExportFormat::json)) {
        preview += "{\n";
        preview += "  \"baseId\": \"" + QString::fromStdString(currentReport_.baseId) + "\",\n";
        preview += "  \"targetId\": \"" + QString::fromStdString(currentReport_.targetId) + "\",\n";
        preview += "  \"items\": [\n";
        for (std::size_t i = 0; i < currentReport_.items.size(); ++i) {
            const auto& item = currentReport_.items[i];
            preview += "    {\n";
            preview += "      \"entityKey\": \"" + QString::fromStdString(item.entityKey) + "\",\n";
            preview += "      \"kind\": \"" + QString::number(static_cast<int>(item.kind)) + "\"\n";
            preview += "    }" + QString(i + 1 < currentReport_.items.size() ? "," : "") + "\n";
        }
        preview += "  ]\n";
        preview += "}";
    } else {
        preview += "<html><body>\n";
        preview += "<h1>Code Xray — Comparison Report (DEMO)</h1>\n";
        preview += "<p>Base: " + QString::fromStdString(currentReport_.baseId) + "</p>\n";
        preview += "<p>Target: " + QString::fromStdString(currentReport_.targetId) + "</p>\n";
        preview += "<table border='1'><tr><th>Function</th><th>Kind</th></tr>\n";
        for (const auto& item : currentReport_.items) {
            preview += "<tr><td>" + QString::fromStdString(item.entityKey) +
                       "</td><td>" + QString::number(static_cast<int>(item.kind)) + "</td></tr>\n";
        }
        preview += "</table>\n</body></html>";
    }

    previewArea_->setPlainText(preview);
    statusLabel_->setText(QString::fromUtf8("Попередній перегляд сформовано (DEMO)."));
}

// D-H12
void ExportPage::onExportRequested()
{
    if (!hasReport_) {
        QMessageBox::warning(this, QString::fromUtf8("Експорт"),
                             QString::fromUtf8("Немає звіту для експорту."));
        return;
    }

    auto path = pathEdit_->text();
    if (path.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("Експорт"),
                             QString::fromUtf8("Вкажіть шлях для збереження."));
        return;
    }

    // Перевірка перезапису
    if (QFile::exists(path)) {
        auto answer = QMessageBox::question(this, QString::fromUtf8("Перезаписати?"),
            QString::fromUtf8("Файл '%1' вже існує. Перезаписати?").arg(path));
        if (answer != QMessageBox::Yes) return;
    }

    // DEMO: реальний експорт буде через exportReport() з review/api.hpp
    statusLabel_->setText(
        QString::fromUtf8("Експорт у '%1' (DEMO — реальний запис потребує координатора).")
            .arg(path));
}

} // namespace xray::ui::review
