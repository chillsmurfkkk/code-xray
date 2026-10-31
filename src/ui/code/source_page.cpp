#include "source_page.hpp"

#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace xray::ui {

SourcePage::SourcePage(QWidget* parent) : QWidget(parent) {
    setObjectName("sourcePage");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel(QString::fromUtf8("Джерело"), this);
    auto titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 7);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    layout->addWidget(title);
    auto* description = new QLabel(QString::fromUtf8(
        "Відкрийте всю папку з C++ кодом або виберіть окремі файли всередині неї."), this);
    description->setObjectName("sourceContext");
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* panel = new QFrame(this);
    panel->setObjectName("workPanel");
    auto* body = new QVBoxLayout(panel);
    body->setContentsMargins(16, 16, 16, 16);
    body->setSpacing(12);
    layout->addWidget(panel, 1);
    auto* scroll = new QScrollArea(panel);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    auto* formWidget = new QWidget(scroll);
    auto* form = new QVBoxLayout(formWidget);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(12);
    scroll->setWidget(formWidget);
    body->addWidget(scroll, 1);

    auto* pathLabel = new QLabel(QString::fromUtf8("Коренева &папка:"), panel);
    directoryInput_ = new QLineEdit(panel);
    directoryInput_->setObjectName("sourceDirectory");
    directoryInput_->setProperty("role", "inputField");
    directoryInput_->setMinimumHeight(36);
    directoryInput_->setPlaceholderText(QString::fromUtf8("Введіть шлях або виберіть папку"));
    directoryInput_->setAccessibleName(QString::fromUtf8("Шлях до кореневої папки"));
    pathLabel->setBuddy(directoryInput_);
    form->addWidget(pathLabel);
    form->addWidget(directoryInput_);

    const auto makeButton = [panel](const char* text, const char* name) {
        auto* button = new QPushButton(QString::fromUtf8(text), panel);
        button->setObjectName(name);
        button->setProperty("role", "primaryAction");
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };
    browseButton_ = makeButton("Вибрати папку…", "sourceBrowseDirectory");
    filesButton_ = makeButton("Додати файли…", "sourceBrowseFiles");
    auto* browseRow = new QHBoxLayout;
    browseRow->addWidget(browseButton_);
    browseRow->addWidget(filesButton_);
    browseRow->addStretch();
    form->addLayout(browseRow);

    selectionLabel_ = new QLabel(panel);
    selectionLabel_->setObjectName("sourceSelection");
    selectionLabel_->setWordWrap(true);
    form->addWidget(selectionLabel_);
    filesList_ = new QListWidget(panel);
    filesList_->setObjectName("sourceFiles");
    filesList_->setAccessibleName(QString::fromUtf8("Вибрані файли відносно кореневої папки"));
    filesList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    filesList_->setMinimumHeight(96);
    form->addWidget(filesList_, 1);

    removeButton_ = makeButton("Прибрати вибрані", "sourceRemoveFiles");
    clearButton_ = makeButton("Уся папка", "sourceClearFiles");
    clearButton_->setToolTip(QString::fromUtf8("Очистити список і сканувати всю папку"));
    auto* selectionRow = new QHBoxLayout;
    selectionRow->addWidget(removeButton_);
    selectionRow->addWidget(clearButton_);
    selectionRow->addStretch();
    form->addLayout(selectionRow);
    form->addStretch();

    scanButton_ = makeButton("Сканувати", "sourceScan");
    cancelButton_ = makeButton("Скасувати", "sourceCancel");
    auto* actionsRow = new QHBoxLayout;
    actionsRow->addWidget(scanButton_);
    actionsRow->addWidget(cancelButton_);
    actionsRow->addStretch();
    body->addLayout(actionsRow);

    progress_ = new QProgressBar(panel);
    progress_->setObjectName("sourceProgress");
    progress_->setAccessibleName(QString::fromUtf8("Прогрес відкриття джерела"));
    body->addWidget(progress_);
    status_ = new QLabel(QString::fromUtf8("Виберіть кореневу папку для початку."), panel);
    status_->setObjectName("sourceStatus");
    status_->setWordWrap(true);
    status_->setTextFormat(Qt::PlainText);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->addWidget(status_);
    structureButton_ = makeButton("Перейти до структури", "sourceOpenStructure");
    body->addWidget(structureButton_, 0, Qt::AlignLeft);

    connect(browseButton_, &QPushButton::clicked, this, &SourcePage::browseDirectoryRequested);
    connect(filesButton_, &QPushButton::clicked, this, &SourcePage::browseFilesRequested);
    connect(directoryInput_, &QLineEdit::textChanged, this, [this] {
        filesList_->clear();
        sourceChanged();
    });
    connect(directoryInput_, &QLineEdit::returnPressed, this, &SourcePage::requestScan);
    connect(filesList_, &QListWidget::itemSelectionChanged, this, &SourcePage::updateActions);
    connect(removeButton_, &QPushButton::clicked, this, [this] {
        for (auto* item : filesList_->selectedItems()) delete item;
        sourceChanged();
    });
    connect(clearButton_, &QPushButton::clicked, this, [this] {
        filesList_->clear();
        sourceChanged();
    });
    connect(scanButton_, &QPushButton::clicked, this, &SourcePage::requestScan);
    connect(cancelButton_, &QPushButton::clicked, this, [this] {
        setCancelling();
        emit cancelRequested();
    });
    connect(structureButton_, &QPushButton::clicked, this, &SourcePage::structureRequested);
    progress_->hide();
    structureButton_->hide();
    updateActions();
}

QString SourcePage::directory() const {
    return directoryInput_->text();
}

code::SourceRequest SourcePage::sourceRequest() const {
    code::SourceRequest request;
    request.root = std::filesystem::path(directory().toStdU16String());
    for (int i = 0; i < filesList_->count(); ++i) {
        request.selection.relativePaths.push_back(filesList_->item(i)->text().toUtf8().toStdString());
    }
    return request;
}

void SourcePage::setDirectory(const QString& directory) {
    if (loading_) return;
    directoryInput_->setText(directory);
    filesList_->clear();
    sourceChanged();
}

void SourcePage::addFiles(const QStringList& paths) {
    if (loading_ || paths.isEmpty()) return;
    const QDir root(directory());
    if (directory().trimmed().isEmpty() || !QFileInfo(directory()).isDir()) {
        showResult(QString::fromUtf8("Спочатку виберіть наявну кореневу папку."));
        return;
    }
    QStringList relativePaths;
    for (const auto& path : paths) {
        const auto relative = QDir::fromNativeSeparators(root.relativeFilePath(QFileInfo(path).absoluteFilePath()));
        if (QDir::isAbsolutePath(relative) || relative == ".." || relative.startsWith("../")
            || !QFileInfo(path).isFile()) {
            showResult(QString::fromUtf8("Файл має бути всередині кореневої папки: %1").arg(path));
            return;
        }
        relativePaths.push_back(relative);
    }
    for (const auto& path : relativePaths) {
        if (filesList_->findItems(path, Qt::MatchExactly).isEmpty()) {
            auto* item = new QListWidgetItem(path, filesList_);
            item->setToolTip(root.absoluteFilePath(path));
        }
    }
    filesList_->sortItems();
    sourceChanged();
}

void SourcePage::requestScan() {
    if (loading_) return;
    const QFileInfo root(directory());
    if (directory().trimmed().isEmpty() || !root.exists() || !root.isDir() || !root.isReadable()) {
        showResult(QString::fromUtf8("Укажіть наявну папку, доступну для читання."));
        directoryInput_->setFocus();
        return;
    }
    emit scanRequested();
}

void SourcePage::sourceChanged() {
    showResult(directory().trimmed().isEmpty()
        ? QString::fromUtf8("Виберіть кореневу папку для початку.")
        : QString::fromUtf8("Джерело змінено. Результат у «Структурі» оновиться після успішного сканування."));
    updateActions();
}

void SourcePage::setLoading(bool loading) {
    loading_ = loading;
    cancelling_ = false;
    cancelButton_->setText(QString::fromUtf8("Скасувати"));
    progress_->setVisible(loading);
    if (loading) {
        structureButton_->hide();
        progress_->setRange(0, 0);
        status_->setText(QString::fromUtf8("Готуємо відкриття джерела…"));
    }
    updateActions();
}

void SourcePage::setProgress(const Progress& progress) {
    if (!loading_ || cancelling_) return;
    const QString stage = progress.stage == "scan-directory" ? QString::fromUtf8("Обхід папки")
        : progress.stage == "collect-files" ? QString::fromUtf8("Читання файлів")
        : progress.stage == "parse-files" ? QString::fromUtf8("Розбір C++")
        : QString::fromStdString(progress.stage);
    const auto completed = static_cast<qulonglong>(progress.completed);
    if (progress.total) {
        const auto total = static_cast<qulonglong>(*progress.total);
        status_->setText(QString::fromUtf8("%1: %2 / %3").arg(stage).arg(completed).arg(total));
        progress_->setRange(0, 1000);
        const auto fraction = *progress.total == 0 ? 1.0L
            : std::min(1.0L, static_cast<long double>(progress.completed) / *progress.total);
        progress_->setValue(static_cast<int>(fraction * 1000));
    } else {
        status_->setText(QString::fromUtf8("%1: опрацьовано %2").arg(stage).arg(completed));
        progress_->setRange(0, 0);
    }
}

void SourcePage::setCancelling() {
    if (!loading_) return;
    cancelling_ = true;
    cancelButton_->setText(QString::fromUtf8("Скасовуємо…"));
    status_->setText(QString::fromUtf8("Очікуємо завершення скасування…"));
    updateActions();
}

void SourcePage::showResult(const QString& message, bool canOpenStructure) {
    status_->setText(message);
    structureButton_->setVisible(canOpenStructure && !loading_);
}

void SourcePage::updateActions() {
    const bool hasDirectory = !directory().trimmed().isEmpty();
    const int count = filesList_->count();
    directoryInput_->setEnabled(!loading_);
    browseButton_->setEnabled(!loading_);
    filesButton_->setEnabled(!loading_ && hasDirectory);
    scanButton_->setEnabled(!loading_ && hasDirectory);
    filesList_->setEnabled(!loading_);
    filesList_->setVisible(count > 0);
    removeButton_->setVisible(count > 0);
    clearButton_->setVisible(count > 0);
    removeButton_->setEnabled(!loading_ && !filesList_->selectedItems().isEmpty());
    clearButton_->setEnabled(!loading_);
    cancelButton_->setVisible(loading_);
    cancelButton_->setEnabled(loading_ && !cancelling_);
    selectionLabel_->setText(count == 0
        ? QString::fromUtf8("Уся папка · стандартні розширення C++, без .git, build, vendor і third_party.")
        : QString::fromUtf8("Лише вибрані файли: %1. Стандартні фільтри C++ також застосовуються.").arg(count));
}

} // namespace xray::ui
