#include "ui/analysis/profiles_page.hpp"
#include "ui/analysis/presentation.hpp"
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace xray::ui::analysis_pages {
ProfilesPage::ProfilesPage(Session& session, QWidget* parent) : QWidget(parent), session_(session) {
    setObjectName("profilesPage");
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Профілі перевірок")));
    auto* help = new QLabel(tr("Збережіть профіль у JSON, щоб відкрити його наступного разу."));
    help->setWordWrap(true);
    layout->addWidget(help);
    list_ = new QListWidget;
    list_->setObjectName("profilesList");
    layout->addWidget(list_);
    auto* buttons = new QHBoxLayout;
    auto addButton = [&](const char* id, const QString& title, auto handler) {
        auto* button = new QPushButton(title);
        button->setObjectName(id);
        button->setProperty("role", "primaryAction");
        buttons->addWidget(button);
        connect(button, &QPushButton::clicked, this, handler);
    };
    addButton("createProfile", tr("Створити"), &ProfilesPage::onProfileCreated);
    addButton("duplicateProfile", tr("Дублювати"), &ProfilesPage::onProfileDuplicated);
    addButton("importProfile", tr("Імпортувати…"), &ProfilesPage::onProfileImported);
    addButton("saveProfile", tr("Зберегти…"), &ProfilesPage::onProfileSaved);
    buttons->addStretch();
    layout->addLayout(buttons);
    connect(list_, &QListWidget::currentRowChanged, &session_, &Session::selectProfile);
    connect(&session_, &Session::profileChanged, this, &ProfilesPage::refresh);
    refresh();
}

void ProfilesPage::refresh() {
    const QSignalBlocker blocker(list_);
    list_->clear();
    for (const auto& profile : session_.profiles()) {
        auto* item = new QListWidgetItem(text(profile.name), list_);
        item->setToolTip(text(profile.id));
    }
    list_->setCurrentRow(session_.selected());
}

void ProfilesPage::onProfileCreated() {
    bool ok = false;
    const auto name =
        QInputDialog::getText(this, tr("Новий профіль"), tr("Назва:"), QLineEdit::Normal, {}, &ok);
    if (ok)
        session_.createProfile(name);
}

void ProfilesPage::onProfileDuplicated() {
    bool ok = false;
    const auto name = QInputDialog::getText(this, tr("Копія профілю"), tr("Назва:"), QLineEdit::Normal,
                                            text(session_.profile().name) + tr(" — копія"), &ok);
    if (ok)
        session_.duplicateProfile(name);
}

void ProfilesPage::onProfileImported() {
    const auto path = QFileDialog::getOpenFileName(this, tr("Імпорт профілю"), {}, tr("JSON (*.json)"));
    if (!path.isEmpty())
        session_.importProfile(filePath(path));
}

void ProfilesPage::onProfileSaved() {
    if (!session_.draftValid()) {
        session_.reportMessage(tr("Виправте пороги на сторінці «Правила»."));
        return;
    }
    const auto path =
        QFileDialog::getSaveFileName(this, tr("Збереження профілю"), "profile.json", tr("JSON (*.json)"));
    if (!path.isEmpty())
        session_.saveProfile(filePath(path));
}
} // namespace xray::ui::analysis_pages
