#include "app/main_window.hpp"
#include "ui/code/source_page.hpp"
#include "ui/shared/popup_style.hpp"
#include "ui/shared/theme.hpp"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QScrollBar>
#include <QStackedWidget>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class T> T* child(QObject& parent, const char* name) {
    auto* result = parent.findChild<T*>(name);
    require(result != nullptr, name);
    return result;
}

void writeFile(const QString& path, const QByteArray& contents) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "Could not create fixture");
    require(file.write(contents) == contents.size(), "Could not write fixture");
}

void waitFor(const std::function<bool()>& predicate) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < 10000) QTest::qWait(5);
    require(predicate(), "Source operation did not finish within 10 seconds");
}
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("CodeXrayGuiTests");
    app.setApplicationName("Source");
    xray::ui::installPopupStyle(app);
    xray::ui::applyTheme(app, xray::ui::Theme::dark);
    const QString output = app.arguments().value(1);
    if (!output.isEmpty()) require(QDir().mkpath(output), "Could not create render directory");
    QTemporaryDir fixture((output.isEmpty() ? QDir::tempPath() : output) + "/fixture-XXXXXX");
    QTemporaryDir outside((output.isEmpty() ? QDir::tempPath() : output) + "/outside-XXXXXX");
    QTemporaryDir settings((output.isEmpty() ? QDir::tempPath() : output) + "/settings-XXXXXX");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    try {
        require(fixture.isValid() && outside.isValid() && settings.isValid(), "Temporary directories must exist");
        using namespace xray::ui;
        const QString root = fixture.path();
        require(QDir(root).mkpath("src"), "Could not create nested directory");
        require(QDir(root).mkpath("empty"), "Could not create empty directory");
        const auto first = root + QString::fromUtf8("/src/привіт.cpp");
        const auto second = root + "/other.cpp";
        writeFile(first, "int square(int x) { return x * x; }\n");
        writeFile(second, "void other() {}\n");
        writeFile(outside.path() + "/outside.cpp", "void outside() {}\n");

        SourcePage page;
        auto* scan = child<QPushButton>(page, "sourceScan");
        auto* cancel = child<QPushButton>(page, "sourceCancel");
        auto* files = child<QListWidget>(page, "sourceFiles");
        auto* input = child<QLineEdit>(page, "sourceDirectory");
        auto* status = child<QLabel>(page, "sourceStatus");
        auto* progress = child<QProgressBar>(page, "sourceProgress");
        require(!scan->isEnabled() && cancel->isHidden(), "Empty source must not scan or cancel");
        QSignalSpy scanRequests(&page, &SourcePage::scanRequested);
        QSignalSpy cancelRequests(&page, &SourcePage::cancelRequested);
        page.setDirectory(root);
        page.addFiles({first, first, second});
        require(files->count() == 2, "Selected files must be deduplicated");
        const auto request = page.sourceRequest();
        require(request.selection.relativePaths.size() == 2
            && request.selection.relativePaths.back() == QString::fromUtf8("src/привіт.cpp").toUtf8().toStdString(),
            "Request must contain normalized relative UTF-8 paths");
        page.addFiles({root + "/missing.cpp", outside.path() + "/outside.cpp"});
        require(files->count() == 2, "Invalid additions must leave the selection intact");
        page.addFiles({first, outside.path() + "/outside.cpp"});
        require(files->count() == 2 && status->text().contains(QString::fromUtf8("всередині")),
            "Files outside the root must be rejected atomically");
        files->item(0)->setSelected(true);
        child<QPushButton>(page, "sourceRemoveFiles")->click();
        require(files->count() == 1, "Selected list entries must be removable");
        child<QPushButton>(page, "sourceClearFiles")->click();
        require(page.sourceRequest().selection.relativePaths.empty(), "Clearing must restore directory mode");
        scan->click();
        require(scanRequests.count() == 1, "Valid source must emit a scan request");
        page.setDirectory(first);
        scan->click();
        require(scanRequests.count() == 1, "A regular file must not be accepted as a root directory");
        page.setDirectory(root + "/missing");
        scan->click();
        require(scanRequests.count() == 1, "Missing root must not emit a scan request");
        page.setDirectory(root);
        page.addFiles({first});
        input->setText(root + "/empty");
        require(files->count() == 0, "Editing the root must clear obsolete relative paths");
        page.setDirectory(root);
        page.setLoading(true);
        require(!scan->isEnabled() && !input->isEnabled() && cancel->isEnabled(), "Loading must lock inputs");
        page.addFiles({first});
        require(files->count() == 0, "Selection cannot change while loading");
        page.setProgress({1, "parse-files", 1, 2});
        require(progress->value() == 500 && status->text().contains("1 / 2"), "Progress must show stage counts");
        page.setProgress({1, "scan-directory", 5, {}});
        require(progress->maximum() == 0, "Unknown totals must use indeterminate progress");
        page.setProgress({1, "parse-files", 0, 0});
        require(progress->value() == 1000, "An empty stage must not divide by zero");
        cancel->click();
        const auto cancellationMessage = status->text();
        page.setProgress({1, "parse-files", 2, 2});
        require(cancelRequests.count() == 1 && !cancel->isEnabled()
            && status->text() == cancellationMessage, "Late progress must not replace cancellation status");
        page.setLoading(false);
        require(scan->isEnabled() && input->isEnabled() && cancel->isHidden(), "Completion must restore actions");

        xray::app::MainWindow window(Theme::dark);
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(1180, 760);
        window.show();
        auto* navigation = child<QListWidget>(window, "pageNavigation");
        navigation->setCurrentRow(0);
        auto* source = child<SourcePage>(window, "sourcePage");
        auto* start = child<QPushButton>(*source, "sourceScan");
        auto* stop = child<QPushButton>(*source, "sourceCancel");
        auto* message = child<QLabel>(*source, "sourceStatus");
        auto* tree = child<QTreeWidget>(window, "structureTree");
        source->setDirectory(root);
        source->addFiles({first});
        start->click();
        require(!start->isEnabled(), "Scan must actually enter the loading state");
        waitFor([&] { return start->isEnabled(); });
        require(message->text().startsWith(QString::fromUtf8("Готово")) && tree->topLevelItemCount() == 1,
            "Background scanning must parse only selected files and restore controls");
        const auto previousFile = tree->topLevelItem(0)->text(0);
        child<QPushButton>(*source, "sourceOpenStructure")->click();
        require(navigation->currentRow() == 2, "Open structure must synchronize navigation");
        navigation->setCurrentRow(0);

        source->setDirectory(root + "/missing");
        require(QMetaObject::invokeMethod(source, "scanRequested", Qt::DirectConnection), "Scan signal must exist");
        waitFor([&] { return start->isEnabled(); });
        require(message->text().startsWith(QString::fromUtf8("Не вдалося"))
            && tree->topLevelItem(0)->text(0) == previousFile,
            "A worker error must preserve the previous snapshot and unlock retry");

        source->setDirectory(root);
        const auto cancellation = QObject::connect(source, &SourcePage::scanRequested, source, [stop] { stop->click(); });
        start->click();
        QObject::disconnect(cancellation);
        waitFor([&] { return start->isEnabled(); });
        require(message->text().contains(QString::fromUtf8("скасовано"))
            && tree->topLevelItemCount() == 1 && tree->topLevelItem(0)->text(0) == previousFile,
            "Cancellation must preserve the old snapshot, even if the worker already completed");
        start->click();
        waitFor([&] { return start->isEnabled(); });
        require(tree->topLevelItemCount() == 2, "Retry must scan the entire directory after clearing selection");

        const auto broken = root + "/broken.cpp";
        writeFile(broken, "int broken( { return ;\n");
        source->addFiles({broken});
        start->click();
        waitFor([&] { return start->isEnabled(); });
        require(message->text().startsWith(QString::fromUtf8("Частковий результат")),
            "Partial parsing must be explicitly distinguished from full success");

        source->setDirectory(root + "/empty");
        start->click();
        waitFor([&] { return start->isEnabled(); });
        require(message->text().contains(QString::fromUtf8("файлів немає")) && tree->topLevelItemCount() == 0,
            "An empty valid root must be a successful empty result");
        navigation->setCurrentRow(2);
        for (int i = 0; i < 100; ++i)
            new QTreeWidgetItem(tree, QStringList{QString("item-%1").arg(i)});
        tree->setEnabled(true);
        child<QStackedWidget>(window, "structureContent")->setCurrentWidget(tree);
        app.processEvents();
        auto* treeScroll = tree->verticalScrollBar();
        require(treeScroll->maximum() > 0 && treeScroll->width() <= 12,
            "Structure tree must have a narrow scrollbar when its contents overflow");
        treeScroll->setValue(0);
        treeScroll->triggerAction(QAbstractSlider::SliderSingleStepAdd);
        require(treeScroll->value() > 0, "Styled scrollbar must still scroll");
        for (const auto theme : {Theme::dark, Theme::light}) {
            applyTheme(app, theme);
            app.processEvents();
            require(treeScroll->width() <= 12 && treeScroll->maximum() > 0,
                "Scrollbar must keep its size and range after changing theme");
            if (!output.isEmpty()) require(window.grab().save(output + (theme == Theme::dark
                ? "/scroll-dark.png" : "/scroll-light.png")), "Could not render scrollbar");
        }
        navigation->setCurrentRow(0);
        source->setDirectory(root);
        source->addFiles({first, second});
        source->showResult(QString::fromUtf8("Готово · контрольний вигляд сторінки."), true);
        for (const auto theme : {Theme::dark, Theme::light}) {
            applyTheme(app, theme);
            app.processEvents();
            if (!output.isEmpty()) require(window.grab().save(output + (theme == Theme::dark ? "/source-dark.png" : "/source-light.png")),
                "Could not render source page");
        }
        window.resize(800, 540);
        auto largeFont = app.font();
        largeFont.setPointSizeF(largeFont.pointSizeF() + 5);
        app.setFont(largeFont);
        app.processEvents();
        require(source->rect().contains(start->mapTo(source, start->rect().center())),
            "Scan action must remain on screen in a narrow window with a larger font");
        if (!output.isEmpty()) require(window.grab().save(output + "/source-narrow-large.png"), "Could not render narrow source");
        source->setLoading(true);
        source->setProgress({1, "parse-files", 1, 2});
        app.processEvents();
        require(source->rect().contains(stop->mapTo(source, stop->rect().center())),
            "Cancellation must remain on screen when the selection form scrolls");
        if (!output.isEmpty()) require(window.grab().save(output + "/source-progress.png"), "Could not render progress");
        source->setLoading(false);
        window.close();

        // Closing a window with an outstanding worker must request stop and join safely.
        {
            xray::app::MainWindow closing(Theme::light);
            auto* closingSource = child<SourcePage>(closing, "sourcePage");
            closingSource->setDirectory(root);
            child<QPushButton>(*closingSource, "sourceScan")->click();
        }
        app.processEvents();
        std::cout << "Source selection, progress, background completion, cancellation, errors, retry and rendering: passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
