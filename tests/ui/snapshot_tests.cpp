#include "code/api.hpp"
#include "ui/code/demo_snapshot.hpp"
#include "ui/code/structure_page.hpp"
#include "ui/shared/theme.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTreeWidget>

#include <algorithm>
#include <filesystem>
#include <future>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void writeFile(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "Cannot create source fixture");
    require(file.write(bytes) == bytes.size(), "Cannot write source fixture");
}

QTreeWidgetItem* findFile(QTreeWidget& tree, const QString& path) {
    for (int i = 0; i < tree.topLevelItemCount(); ++i) {
        auto* item = tree.topLevelItem(i);
        if (item->text(0) == path) return item;
    }
    throw std::runtime_error("Source file is missing from the GUI tree");
}

QTreeWidgetItem* findFunction(QTreeWidgetItem* file, const QString& name) {
    for (int i = 0; i < file->childCount(); ++i) {
        if (file->child(i)->text(0) == name) return file->child(i);
    }
    throw std::runtime_error("Function is missing from the GUI tree");
}

QString displayedText(const QByteArray& bytes) {
    auto text = QString::fromUtf8(bytes);
    if (text.startsWith(QChar::ByteOrderMark)) text.remove(0, 1);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    // QTextDocument exports these separators as ordinary newlines.
    text.replace(QChar(0x2029), QLatin1Char('\n'));
    text.replace(QChar(0x2028), QLatin1Char('\n'));
    return text;
}

void checkEntityFilters(const QString& directory) {
    using namespace xray;
    using namespace xray::ui;
    require(QDir().mkpath(directory + "/filters"), "Cannot create filter fixtures");
    writeFile(directory + "/filters/alpha.cpp",
              "struct Widget { int tick() { return 1; } int idle() { return 0; } };\n");
    writeFile(directory + "/filters/beta.cpp", "int other() { return 2; }\n");
    code::SourceRequest request;
    request.root = std::filesystem::path(directory.toStdU16String());
    request.selection.relativePaths = {"filters/alpha.cpp", "filters/beta.cpp"};
    auto result = std::async(std::launch::async, [&] {
        return code::collectAndParse(request, {}, {});
    }).get();
    require(std::holds_alternative<code::ParsedSnapshot>(result), "Filter fixtures must parse");
    auto snapshot = std::make_shared<const code::ParsedSnapshot>(
        std::get<code::ParsedSnapshot>(std::move(result)));

    StructurePage page;
    auto* search = page.findChild<QLineEdit*>("entitySearch");
    auto* kind = page.findChild<QComboBox*>("entityKindFilter");
    auto* tree = page.findChild<QTreeWidget*>("structureTree");
    auto* count = page.findChild<QLabel*>("countPill");
    auto* stack = page.findChild<QStackedWidget*>("structureContent");
    auto* editor = page.findChild<QPlainTextEdit*>("sourceCode");
    auto* reset = page.findChild<QPushButton*>("resetEntityFilters");
    require(search && kind && tree && count && stack && editor && reset, "Missing filter controls");
    auto* noSource = stack->currentWidget();
    search->setText("not present");
    require(stack->currentWidget() == noSource && count->text() == QString::fromUtf8("—"),
            "Filtering without a source must retain its initial state");
    search->clear();
    page.setSnapshot(snapshot);
    require(count->text() == "6", "The unfiltered tree must count files, types and functions");
    auto* alpha = findFile(*tree, "filters/alpha.cpp");
    auto* beta = findFile(*tree, "filters/beta.cpp");
    auto* type = alpha->child(0);
    auto* tick = findFunction(type, "Widget::tick");
    auto* idle = findFunction(type, "Widget::idle");
    tree->setCurrentItem(idle);
    require(!editor->toPlainText().isEmpty(), "The filter fixture must support source navigation");

    // Disk changes cannot alter the already parsed tree or its source bytes.
    writeFile(directory + "/filters/alpha.cpp", "int changed() { return 7; }\n");
    search->setText("  TICK  ");
    require(count->text() == "1" && !alpha->isHidden() && !type->isHidden()
                && !tick->isHidden() && idle->isHidden() && beta->isHidden(),
            "Search must trim whitespace, ignore case and retain only matches and their ancestors");
    require(alpha->isExpanded() && type->isExpanded(), "Search must expose nested matches");
    require(!tree->currentItem() && editor->toPlainText().isEmpty(),
            "Hiding the current entity must clear its source preview");
    kind->setCurrentIndex(kind->findData(static_cast<int>(code::EntityKind::function)));
    tree->setCurrentItem(tick);
    require(editor->toPlainText().contains("struct Widget"), "Filtering must retain snapshot bytes");
    applyTheme(*qApp, Theme::light);
    require(search->text() == "  TICK  " && tree->currentItem() == tick && count->text() == "1",
            "Changing theme must retain filters and the visible selection");
    search->clear();
    require(count->text() == "3" && !idle->isHidden() && !beta->isHidden(),
            "Function filtering must still visit children of nonmatching types and files");
    kind->setCurrentIndex(kind->findData(static_cast<int>(code::EntityKind::type)));
    require(count->text() == "1" && !type->isHidden() && tick->isHidden() && beta->isHidden(),
            "Type filtering must not expose nonmatching children or sibling files");
    kind->setCurrentIndex(kind->findData(static_cast<int>(code::EntityKind::file)));
    require(count->text() == "2" && !alpha->isHidden() && !beta->isHidden() && type->isHidden(),
            "File filtering must count files and hide their nonmatching children");

    search->setText("no match");
    require(count->text() == "0" && stack->currentWidget()->objectName() == "noFilterMatches",
            "No matches must have a distinct empty state");
    reset->click();
    require(search->text().isEmpty() && !kind->currentData().isValid()
                && stack->currentWidget() == tree && count->text() == "6"
                && tree->topLevelItem(0) == alpha && !idle->isHidden(),
            "Reset must restore all entities without rebuilding the tree");
    search->setText("tick");
    kind->setCurrentIndex(kind->findData(static_cast<int>(code::EntityKind::file)));
    require(count->text() == "0", "Text and kind criteria must both match the same entity");
    page.setSnapshot(makeDemoSnapshot());
    require(count->text() == "0" && stack->currentWidget()->objectName() == "noFilterMatches",
            "New snapshots must immediately apply the existing filters");
    page.setSnapshot(std::make_shared<const code::ParsedSnapshot>());
    require(stack->currentWidget()->objectName() != "noFilterMatches" && count->text() == "0",
            "An empty snapshot must not be presented as a failed search");
    page.setSnapshot(nullptr);
    require(stack->currentWidget() == noSource, "Clearing the snapshot must restore the no-source state");
}
} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    const QString output = app.arguments().value(1);
    const QString testRoot = output.isEmpty() ? QDir::currentPath() : output;
    QDir().mkpath(testRoot);
    QTemporaryDir directory(testRoot + "/source-XXXXXX");

    try {
        using namespace xray;
        using namespace xray::ui;
        require(directory.isValid(), "The snapshot test directory must be writable");
        require(QDir().mkpath(directory.path() + "/src"), "Cannot create fixture folder");
        const QString path = QString::fromUtf8("src/приклад.cpp");
        QByteArray original = QByteArray::fromHex("efbbbf");
        original += QString::fromUtf8("// Привіт 😀\u2029абзац\u2028продовження\r\n").toUtf8();
        for (int i = 0; i < 60; ++i) original += "// context\r\n";
        original += "\r\nint first() {\r\n    return 1;\r\n}\r\n";
        original += "\r\nint second() {\r\n    return 2;\r\n}\r\n";
        writeFile(directory.path() + '/' + path, original);
        writeFile(directory.path() + "/src/other.cpp", "int first() { return 9; }\n");
        writeFile(directory.path() + "/src/empty.cpp", {});

        auto result = std::async(std::launch::async, [&] {
            const auto rootBytes = directory.path().toUtf8();
            code::SourceRequest request;
            request.root = std::filesystem::u8path(rootBytes.constData());
            const JobContext context;
            auto collected = code::collect(request, context);
            require(std::holds_alternative<code::SourceSnapshot>(collected), "Source collection failed");
            auto source = std::make_shared<const code::SourceSnapshot>(
                std::get<code::SourceSnapshot>(std::move(collected)));
            return code::parse(source, code::ParseOptions{}, context);
        }).get();
        require(std::holds_alternative<code::ParsedSnapshot>(result), "C++ parsing failed");
        auto parsed = std::make_shared<const code::ParsedSnapshot>(
            std::get<code::ParsedSnapshot>(std::move(result)));
        require(parsed->units.size() == 3, "All source files, including an empty file, must be parsed");

        applyTheme(app, Theme::dark);
        StructurePage page;
        page.setAttribute(Qt::WA_DontShowOnScreen);
        page.resize(920, 600);
        page.show();
        app.processEvents();
        auto* tree = page.findChild<QTreeWidget*>("structureTree");
        auto* editor = page.findChild<QPlainTextEdit*>("sourceCode");
        auto* sourcePath = page.findChild<QLineEdit*>("sourcePath");
        auto* context = page.findChild<QLabel*>("sourceContext");
        auto* content = page.findChild<QStackedWidget*>("sourceContent");
        auto* openButton = page.findChild<QPushButton*>("openSourceButton");
        auto* cancelButton = page.findChild<QPushButton*>("cancelSourceButton");
        require(tree && editor && sourcePath && context && content && openButton && cancelButton,
                "Missing snapshot widgets");

        page.setSnapshot(parsed);
        app.processEvents();
        require(tree->topLevelItemCount() == 3, "All source files must appear in the tree");
        require(!context->text().contains(QString::fromUtf8("Демонстраційні")), "Real results must not be marked as demo");
        auto* file = findFile(*tree, path);
        auto* first = findFunction(file, "first");
        auto* second = findFunction(file, "second");
        const auto expected = displayedText(original);

        // Changing the working tree must not change the displayed snapshot.
        writeFile(directory.path() + '/' + path, "int changed_on_disk() { return 0; }\n");
        tree->setCurrentItem(file);
        app.processEvents();
        require(editor->toPlainText() == expected, "Viewer must use immutable snapshot bytes");
        require(editor->isReadOnly() && sourcePath->isReadOnly(), "Source and path must be read-only");
        require(sourcePath->text() == path && sourcePath->toolTip() == path, "The full path must be available");
        require(editor->extraSelections().isEmpty(), "Selecting a file must not invent a range");
        const auto baselineGutter = tree->viewport()->grab().toImage();

        int contentChanges = 0;
        QObject::connect(editor->document(), &QTextDocument::contentsChanged, &page,
                         [&] { ++contentChanges; });

        tree->setCurrentItem(first);
        app.processEvents();
        require(contentChanges == 0, "Changing entity within a file must reuse the displayed document");
        const int firstPosition = static_cast<int>(expected.indexOf("int first"));
        require(editor->textCursor().position() == firstPosition,
                "UTF-8, CRLF, BOM and Unicode separators must not shift navigation");
        const auto firstHighlight = editor->extraSelections();
        require(!firstHighlight.isEmpty(), "Function selection must mark its range");
        require(firstHighlight.front().cursor.selectionStart() == firstPosition,
                "Function range must start at its source line");
        require(firstHighlight.front().cursor.selectionEnd() == expected.indexOf("}\n", firstPosition) + 1,
                "Function range must include its final source line");
        require(!editor->textCursor().hasSelection(), "Range highlighting must not select user text");

        int cancellationRequests = 0;
        QObject::connect(&page, &StructurePage::cancelRequested, &page, [&] { ++cancellationRequests; });
        require(openButton->isEnabled() && cancelButton->isHidden(), "Initial loading controls are incorrect");
        page.setLoading(true);
        require(!openButton->isEnabled() && !cancelButton->isHidden() && cancelButton->isEnabled(),
                "Loading must expose an enabled cancel button and block a second open");
        cancelButton->click();
        require(cancellationRequests == 1 && !cancelButton->isEnabled(),
                "Cancelling must emit one request and prevent repeated requests");
        require(tree->currentItem() == first && editor->toPlainText() == expected,
                "Loading and cancellation must preserve the previous snapshot");
        page.setLoading(false);
        require(openButton->isEnabled() && cancelButton->isHidden(), "Finishing must restore the open action");

        const auto selectedGutter = tree->viewport()->grab().toImage();
        const QRect itemRect = tree->visualItemRect(first);
        const qreal ratio = tree->viewport()->devicePixelRatioF();
        const int gutterY = qRound(itemRect.center().y() * ratio);
        for (int x = 0; x < qRound(itemRect.left() * ratio); ++x)
            require(baselineGutter.pixel(x, gutterY) == selectedGutter.pixel(x, gutterY),
                    "Selecting a function must not color its branch indentation");

        auto userCursor = editor->textCursor();
        userCursor.setPosition(static_cast<int>(expected.indexOf("return 1")));
        userCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 6);
        editor->setTextCursor(userCursor);
        applyTheme(app, Theme::light);
        app.processEvents();
        require(tree->currentItem() == first && editor->textCursor().selectedText() == "return",
                "Theme changes must preserve entity and user text selection");
        const auto themedSelections = editor->extraSelections();
        require(!themedSelections.isEmpty()
                && std::all_of(themedSelections.begin(), themedSelections.end(), [&](const auto& selection) {
                    return selection.format.background().color() == app.palette().color(QPalette::Highlight);
                }),
                "The range highlight must use the new theme");

        tree->setCurrentItem(second);
        app.processEvents();
        require(contentChanges == 0, "Selecting another entity must not reload the same file");
        require(editor->textCursor().position() == expected.indexOf("int second"), "Navigation to another function failed");
        const auto blockRect = editor->cursorRect();
        require(blockRect.bottom() >= 0 && blockRect.top() < editor->viewport()->height(),
                "The selected function must be scrolled into view");
        auto* otherFile = findFile(*tree, "src/other.cpp");
        tree->setCurrentItem(findFunction(otherFile, "first"));
        app.processEvents();
        require(contentChanges > 0, "Selecting a different file must load its document");
        require(editor->toPlainText() == "int first() { return 9; }\n", "Same-name functions must use the correct file");
        tree->setCurrentItem(otherFile);
        require(editor->extraSelections().isEmpty(), "Selecting a file must clear the old function highlight");
        tree->setCurrentItem(findFile(*tree, "src/empty.cpp"));
        require(content->currentWidget() == editor && editor->toPlainText().isEmpty()
                && sourcePath->text() == "src/empty.cpp", "An empty file must be a valid source view");

        auto partial = std::make_shared<code::ParsedSnapshot>(*parsed);
        partial->units.front().coverage.completeness = Completeness::partial;
        page.setSnapshot(partial);
        require(context->text().contains(QString::fromUtf8("частковий")), "Partial units need an explicit warning");
        require(editor->toPlainText().isEmpty() && sourcePath->text().isEmpty(), "Replacing a result must clear stale source");

        const auto demo = makeDemoSnapshot();
        page.setSnapshot(demo, true);
        require(context->text().contains(QString::fromUtf8("Демонстраційні дані")), "Demo data must be labeled");
        tree->setCurrentItem(tree->topLevelItem(0)->child(0));
        app.processEvents();
        require(!editor->extraSelections().isEmpty(), "Demo function must have a range");
        if (!output.isEmpty()) {
            require(page.grab().save(output + "/demo-light.png"), "Cannot save light source preview");
            applyTheme(app, Theme::dark);
            app.processEvents();
            require(page.grab().save(output + "/demo-dark.png"), "Cannot save dark source preview");
        }

        auto missing = std::make_shared<code::ParsedSnapshot>(*demo);
        auto missingFile = std::make_shared<code::FileEntity>(*missing->units.front().root);
        missingFile->sourceBytes.reset();
        missing->units.front().root = missingFile;
        page.setSnapshot(missing);
        tree->setCurrentItem(tree->topLevelItem(0));
        require(content->currentWidget() != editor && sourcePath->text().isEmpty(), "Unavailable source must not show stale code");

        auto invalidRange = std::make_shared<code::ParsedSnapshot>(*demo);
        auto rangeFile = std::make_shared<code::FileEntity>(*demo->units.front().root);
        auto function = std::make_shared<code::FunctionEntity>(*demo->units.front().functions.front());
        function->range.endLine = 999;
        rangeFile->children = {function};
        invalidRange->units.front().root = rangeFile;
        invalidRange->units.front().functions = {function};
        page.setSnapshot(invalidRange);
        tree->setCurrentItem(tree->topLevelItem(0)->child(0));
        require(editor->extraSelections().isEmpty() && !editor->toPlainText().isEmpty(),
                "An invalid range must not create a false highlight or hide the source");

        auto empty = std::make_shared<code::ParsedSnapshot>();
        page.setSnapshot(empty);
        auto* treeContent = page.findChild<QStackedWidget*>("structureContent");
        auto* emptyWidget = treeContent->currentWidget();
        require(emptyWidget != tree && context->text().contains(QString::fromUtf8("сутностей немає")),
                "An empty result needs its own state");
        page.setSnapshot(nullptr);
        require(treeContent->currentWidget() != emptyWidget && !tree->isEnabled(),
                "No source and an empty result must be distinct");
        require(editor->extraSelections().isEmpty() && sourcePath->text().isEmpty(), "Clearing the result must clear all selection state");

        auto transient = makeDemoSnapshot();
        const std::weak_ptr<const code::ParsedSnapshot> lifetime = transient;
        page.setSnapshot(transient);
        transient.reset();
        require(!lifetime.expired(), "The page must retain the displayed snapshot");
        tree->setCurrentItem(tree->topLevelItem(0)->child(0));
        require(!editor->toPlainText().isEmpty(), "Retained source must remain readable after the caller releases it");
        page.setSnapshot(nullptr);
        require(lifetime.expired(), "Clearing the page must release its old snapshot");
        checkEntityFilters(directory.path());
        std::cout << "Producer-to-GUI snapshots, immutable source, navigation, ranges, themes, filters and empty states: passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
