#include "file_selection_page.hpp"

#include "code/types.hpp"
#include "ui/shared/theme.hpp"

#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QStringList>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>

#include <algorithm>
#include <utility>

namespace xray::ui {

FileSelectionPage::FileSelectionPage(QWidget* parent)
    : QWidget(parent) {
    setObjectName("fileSelectionPage");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel(QString::fromUtf8("Відбір файлів"), this);
    auto titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 7);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* description = new QLabel(QString::fromUtf8(
        "Налаштуйте розширення та каталоги, які застосовуються під час наступного сканування."), this);
    description->setObjectName("sourceContext");
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* panel = new QFrame(this);
    panel->setObjectName("workPanel");
    layout->addWidget(panel, 1);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(16, 16, 16, 16);
    auto* scroll = new QScrollArea(panel);
    scroll->setObjectName("sourceFormScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    styleWorkspaceScrollBar(scroll->verticalScrollBar());
    panelLayout->addWidget(scroll);
    auto* formWidget = new QWidget(scroll);
    formWidget->setObjectName("sourceFormContents");
    auto* form = new QVBoxLayout(formWidget);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(12);
    scroll->setWidget(formWidget);

    auto* extensionsLabel = new QLabel(
        QString::fromUtf8("Розширення через кому"),
        this
    );
    form->addWidget(extensionsLabel);

    extensionsInput_ = new QLineEdit(this);
    extensionsInput_->setObjectName("extensionsInput");
    extensionsInput_->setProperty("role", "inputField");
    extensionsInput_->setMinimumHeight(36);
    extensionsLabel->setBuddy(extensionsInput_);
    form->addWidget(extensionsInput_);

    auto* exclusionsLabel = new QLabel(
        QString::fromUtf8("Виключені каталоги"),
        this
    );
    form->addWidget(exclusionsLabel);

    excludedDirectories_ = new QListWidget(this);
    excludedDirectories_->setObjectName("excludedDirectories");
    excludedDirectories_->setAccessibleName(QString::fromUtf8("Виключені каталоги"));
    excludedDirectories_->setMinimumHeight(120);
    excludedDirectories_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    styleWorkspaceScrollBar(excludedDirectories_->verticalScrollBar());
    exclusionsLabel->setBuddy(excludedDirectories_);
    form->addWidget(excludedDirectories_, 1);

    auto* exclusionRow = new QHBoxLayout;

    auto* exclusionInput = new QLineEdit(this);
    exclusionInput->setObjectName("exclusionInput");
    exclusionInput->setProperty("role", "inputField");
    exclusionInput->setMinimumHeight(36);
    exclusionInput->setAccessibleName(QString::fromUtf8("Назва виключеного каталогу"));
    exclusionInput->setPlaceholderText(
        QString::fromUtf8("Назва каталогу, наприклад build")
    );

    auto* addButton = new QPushButton(QString::fromUtf8("Додати"), this);
    auto* removeButton = new QPushButton(QString::fromUtf8("Видалити вибране"), this);
    addButton->setObjectName("addExclusion");
    removeButton->setObjectName("removeExclusion");
    addButton->setProperty("role", "primaryAction");
    removeButton->setProperty("role", "primaryAction");
    addButton->setCursor(Qt::PointingHandCursor);
    removeButton->setCursor(Qt::PointingHandCursor);

    form->addWidget(exclusionInput);
    exclusionRow->addWidget(addButton);
    exclusionRow->addWidget(removeButton);
    exclusionRow->addStretch();
    form->addLayout(exclusionRow);

    auto* exclusionMessage = new QLabel(this);
    exclusionMessage->setWordWrap(true);
    exclusionMessage->setObjectName("exclusionMessage");
    form->addWidget(exclusionMessage);

    connect(addButton, &QPushButton::clicked, this, [this, exclusionInput, exclusionMessage] {
        const QString name = exclusionInput->text().trimmed();

        if (name.isEmpty() || name == "." || name == ".."
            || name.contains('/') || name.contains('\\')) {
            exclusionMessage->setText(
                QString::fromUtf8("Введіть назву одного каталогу без роздільників шляху.")
            );
            return;
        }

        if (!excludedDirectories_->findItems(name, Qt::MatchExactly).isEmpty()) {
            exclusionMessage->setText(
                QString::fromUtf8("Такий каталог уже є у списку.")
            );
            return;
        }

        excludedDirectories_->addItem(name);
        exclusionInput->clear();
        exclusionMessage->clear();
    });

    connect(removeButton, &QPushButton::clicked, this, [this, exclusionMessage] {
        for (auto* item : excludedDirectories_->selectedItems()) {
            delete item;
        }
        exclusionMessage->clear();
    });

    // Початкові значення беремо з контракту ядра, щоб не дублювати їх.
    const code::FileSelection defaults;

    QStringList extensions;
    for (const auto& extension : defaults.extensions) {
        extensions.append(QString::fromStdString(extension));
    }
    extensionsInput_->setText(extensions.join(", "));

    for (const auto& directory : defaults.excludedDirectories) {
        excludedDirectories_->addItem(QString::fromStdString(directory));
    }

}

bool FileSelectionPage::applyTo(
    code::FileSelection& selection,
    QString& error
) const {
    const QStringList parts = extensionsInput_->text().split(',');

    // Одне розширення починається з крапки та не містить пробілів і шляхів.
    const QRegularExpression validExtension(
        QStringLiteral(R"(^\.[^./\\\s,]+$)")
    );

    std::vector<std::string> extensions;

    for (const QString& part : parts) {
        const QString extension = part.trimmed();

        if (!validExtension.match(extension).hasMatch()) {
            error = QString::fromUtf8(
                "Введіть розширення через кому, наприклад: .cpp, .hpp"
            );
            return false;
        }

        const std::string value = extension.toUtf8().toStdString();
        if (std::find(extensions.begin(), extensions.end(), value) == extensions.end()) {
            extensions.push_back(value);
        }
    }

    std::vector<std::string> exclusions;
    for (int i = 0; i < excludedDirectories_->count(); ++i) {
        exclusions.push_back(
            excludedDirectories_->item(i)->text().toUtf8().toStdString()
        );
    }

    selection.extensions = std::move(extensions);
    selection.excludedDirectories = std::move(exclusions);
    error.clear();
    return true;
}

} // namespace xray::ui
