#include "main_window.hpp"
#include "app/analysis_controller.hpp"
#include "ui/analysis/workspace.hpp"
#include "ui/code/structure_page.hpp"
#include "ui/shared/navigation.hpp"
#include "ui/shared/theme.hpp"

#include "ui/review/ComparisonPage.hpp"
#include "ui/review/TrendPage.hpp"
#include "ui/review/ChangeCardPage.hpp"
#include "ui/review/ExportPage.hpp"
#include "ui/review/review_fixtures.hpp"

#include "ui/history/CommitsPage.hpp"
#include "ui/history/HistoryFiltersPage.hpp"
#include "ui/history/CodeChurnPage.hpp"
#include "ui/history/AncestryPage.hpp"

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
#include <QStackedWidget>

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

    contentStack_ = new QStackedWidget(central);

    // Сторінка розділу Код
    structure_ = new ui::StructurePage(contentStack_);
    contentStack_->addWidget(structure_);

    analysisController_ = new AnalysisController(this);
    analysisPages_ = new ui::analysis_pages::Workspace(analysisController_->session(), contentStack_);
    contentStack_->addWidget(analysisPages_);
    connect(analysisController_, &AnalysisController::progressChanged,
            analysisPages_, &ui::analysis_pages::Workspace::setProgress);
    connect(analysisPages_, &ui::analysis_pages::Workspace::cancelRequested,
            analysisController_, &AnalysisController::cancel);
    if (qApp->arguments().contains("--demo")) {
        setSnapshot(ui::makeDemoSnapshot(), true);
    }

    // Сторінки розділу Порівняння (Модуль D)
    comparisonPage_ = new ui::review::ComparisonPage(contentStack_);
    trendPage_      = new ui::review::TrendPage(contentStack_);
    changeCardPage_ = new ui::review::ChangeCardPage(contentStack_);
    exportPage_     = new ui::review::ExportPage(contentStack_);

    contentStack_->addWidget(comparisonPage_);
    contentStack_->addWidget(trendPage_);
    contentStack_->addWidget(changeCardPage_);
    contentStack_->addWidget(exportPage_);

    // Сторінки розділу Історія (Модуль C)
    commitsPage_        = new ui::history::CommitsPage(contentStack_);
    historyFiltersPage_ = new ui::history::HistoryFiltersPage(contentStack_);
    codeChurnPage_      = new ui::history::CodeChurnPage(contentStack_);
    ancestryPage_       = new ui::history::AncestryPage(contentStack_);

    contentStack_->addWidget(commitsPage_);
    contentStack_->addWidget(historyFiltersPage_);
    contentStack_->addWidget(codeChurnPage_);
    contentStack_->addWidget(ancestryPage_);

    layout->addWidget(navigation);
    layout->addWidget(contentStack_, 1);
    setCentralWidget(central);

    // Навігація між сторінками
    connect(navigation, &ui::NavigationPanel::pageSelected, this, [this](ui::Section section, int pageIndex) {
        if (section == ui::Section::code) {
            if (pageIndex == 2) { // Структура
                contentStack_->setCurrentWidget(structure_);
            }
        } else if (section == ui::Section::analysis) {
            analysisPages_->setCurrentPage(pageIndex);
            contentStack_->setCurrentWidget(analysisPages_);
        } else if (section == ui::Section::history) {
            switch (pageIndex) {
            case 0: contentStack_->setCurrentWidget(commitsPage_);        break;
            case 1: contentStack_->setCurrentWidget(historyFiltersPage_); break;
            case 2: contentStack_->setCurrentWidget(codeChurnPage_);      break;
            case 3: contentStack_->setCurrentWidget(ancestryPage_);       break;
            default: break;
            }
        } else if (section == ui::Section::review) {
            switch (pageIndex) {
            case 0: contentStack_->setCurrentWidget(comparisonPage_); break;
            case 1: contentStack_->setCurrentWidget(trendPage_);      break;
            case 2: contentStack_->setCurrentWidget(changeCardPage_); break;
            case 3: contentStack_->setCurrentWidget(exportPage_);     break;
            default: break;
            }
        }
    });

    // Зв'язуємо вибір рядка в таблиці порівняння з відкриттям «Картки зміни» та синхронізацією панелі [P1]
    connect(comparisonPage_, &ui::review::ComparisonPage::reviewItemSelected, this, [this, navigation](int index) {
        const auto& report = comparisonPage_->currentReport();
        if (index >= 0 && index < static_cast<int>(report.items.size())) {
            changeCardPage_->showItem(report.items[static_cast<std::size_t>(index)], report);
            navigation->setCurrentPage(ui::Section::review, 2);
        }
    });

    connect(
        structure_,
        &ui::StructurePage::openDirectoryRequested,
        this,
        [this]() {
            const QString directory = QFileDialog::getExistingDirectory(
                this,
                QString::fromUtf8("Виберіть папку з кодом")
            );

            if (directory.isEmpty()) {
                return;
            }

            loadDirectory(directory, structure_);
        }
    );

    connect(
        structure_,
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

    analysisController_->setInput(nullptr);

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
                setSnapshot(
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

void MainWindow::setSnapshot(std::shared_ptr<const code::ParsedSnapshot> snapshot, bool demonstration) {
    structure_->setSnapshot(snapshot, demonstration);
    analysisPages_->setContext(!snapshot ? QString::fromUtf8("Відкрийте папку у розділі «Код → Структура».")
        : demonstration ? QString::fromUtf8("Демонстраційний знімок")
                        : QString::fromUtf8("Поточний знімок відкритої папки"));
    analysisController_->setInput(std::move(snapshot));
}

} // namespace xray::app
