#include "main_window.hpp"
#include "ui/code/structure_page.hpp"
#include "ui/shared/navigation.hpp"
#include "ui/shared/theme.hpp"

#include "code/api.hpp"

#include "ui/code/demo_snapshot.hpp"

#include <exception>
#include <filesystem>
#include <memory>
#include <utility>
#include <variant>

#include <QApplication>
#include <QHBoxLayout>
#include <QScreen>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QThread>

namespace xray::app {

MainWindow::MainWindow(ui::Theme theme, QWidget* parent)
    : QMainWindow(parent), theme_(theme) {
    setWindowTitle("Code Xray");
    setMinimumSize(800, 540);
    resize(QSize(1180, 760).boundedTo(screen()->availableGeometry().size() - QSize(48, 64)));

    auto* central = new QWidget(this);
    central->setObjectName("appRoot");
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(16, 16, 20, 16);
    layout->setSpacing(20);

    auto* navigation = new ui::NavigationPanel(central);
    navigation->setTheme(theme_);
    auto* structure = new ui::StructurePage(central);
    if (qApp->arguments().contains("--demo")) {
        structure->setSnapshot(ui::makeDemoSnapshot(), true);
    }
    layout->addWidget(navigation);
    layout->addWidget(structure, 1);
    setCentralWidget(central);

    connect(
        structure,
        &ui::StructurePage::openDirectoryRequested,
        this,
        [this, structure]() {
            const QString directory = QFileDialog::getExistingDirectory(
                this,
                QString::fromUtf8("Виберіть папку з кодом")
            );

            if (directory.isEmpty()) {
                return;
            }

            loadDirectory(directory, structure);
        }
    );

    connect(
        structure,
        &ui::StructurePage::cancelRequested,
        this,
        [this]() {
            if (sourceThread_) {
                sourceStop_.request_stop();

                statusBar()->showMessage(
                    QString::fromUtf8("Скасовуємо відкриття…")
                );
            }
        }
    );

    connect(navigation, &ui::NavigationPanel::themeToggleRequested, this, [this, navigation]() {
        theme_ = theme_ == ui::Theme::dark ? ui::Theme::light : ui::Theme::dark;
        ui::applyTheme(*qApp, theme_);
        ui::saveTheme(theme_);
        navigation->setTheme(theme_);
    });
}

void MainWindow::loadDirectory(
    const QString& directory,
    ui::StructurePage* structure
) {
    if (sourceThread_) {
        return;
    }

    code::SourceRequest request;
    request.root = std::filesystem::path(directory.toStdU16String());

    sourceStop_ = std::stop_source{};
    const auto token = sourceStop_.get_token();

    using LoadResult = Result<code::ParsedSnapshot>;
    auto result = std::make_shared<LoadResult>(Cancelled{});

    structure->setLoading(true);
    statusBar()->showMessage(
        QString::fromUtf8("Збираємо файли та розбираємо код…")
    );

    sourceThread_ = QThread::create(
        [request = std::move(request), token, result]() {
            try {
                JobContext context;
                context.stopToken = token;

                *result = code::collectAndParse(
                    request,
                    code::ParseOptions{},
                    context
                );
            } catch (const std::exception& exception) {
                *result = Error{
                    ErrorCode::read_error,
                    exception.what()
                };
            } catch (...) {
                *result = Error{
                    ErrorCode::read_error,
                    "Unexpected error while opening source"
                };
            }
        }
    );

    sourceThread_->setParent(this);

    connect(
        sourceThread_,
        &QThread::finished,
        this,
        [this, structure, result]() {
            sourceThread_ = nullptr;
            structure->setLoading(false);
            statusBar()->clearMessage();

            if (auto* snapshot =
                    std::get_if<code::ParsedSnapshot>(result.get())) {
                structure->setSnapshot(
                    std::make_shared<const code::ParsedSnapshot>(
                        std::move(*snapshot)
                    )
                );
            } else if (const auto* error =
                           std::get_if<Error>(result.get())) {
                QMessageBox::warning(
                    this,
                    QString::fromUtf8("Не вдалося відкрити джерело"),
                    QString::fromStdString(error->message)
                );
            } else {
                statusBar()->showMessage(
                    QString::fromUtf8("Відкриття скасовано"),
                    5000
                );
            }
        }
    );

    connect(
        sourceThread_,
        &QThread::finished,
        sourceThread_,
        &QObject::deleteLater
    );

    sourceThread_->start();
}

MainWindow::~MainWindow() {
    sourceStop_.request_stop();

    if (sourceThread_) {
        sourceThread_->wait();
    }
}

} // namespace xray::app
