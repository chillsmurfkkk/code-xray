#include "ui/code/code_view.hpp"
#include "ui/shared/theme.hpp"

#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>

#include <iostream>
#include <stdexcept>

namespace {
using namespace xray::ui;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

QTextCharFormat formatAt(const CodeView& view, int position) {
    const auto block = view.document()->findBlock(position);
    require(block.isValid(), "No text block at the requested source position");
    const int local = position - block.position();
    for (const auto& range : block.layout()->formats()) {
        if (local >= range.start && local < range.start + range.length)
            return range.format;
    }
    return {};
}

void expectColor(const CodeView& view, const QString& source, const QString& needle,
                 const QColor& expected, int count = -1) {
    const auto start = source.indexOf(needle);
    require(start >= 0, "The syntax fixture is missing a requested token");
    if (count < 0) count = static_cast<int>(needle.size());
    for (int i = 0; i < count; ++i) {
        const auto format = formatAt(view, static_cast<int>(start) + i);
        const auto actual = format.foreground().style() == Qt::NoBrush
            ? view.palette().color(QPalette::Text) : format.foreground().color();
        if (actual != expected) {
            throw std::runtime_error(QStringLiteral("Wrong color for %1 at %2: %3, expected %4")
                .arg(needle).arg(i).arg(actual.name(), expected.name()).toStdString());
        }
        require(format.background().style() == Qt::NoBrush,
                "Syntax formatting must leave entity and user selection backgrounds available");
    }
}

void load(CodeView& view, const QString& source) {
    view.setSourceText(source);
    QApplication::processEvents();
}

void checkTokensAndTheme(QApplication& app, CodeView& view, const QString& output) {
    const auto source = QString::fromUtf8(R"fixture(/* leading */ #include <dir//return.hpp>
#define LIMIT 1'000u \
    + 0x1.fp+2 // macro comment
int printValue = 42;
auto normal = u8"https://example.com/return";
auto letter = U'😀';
auto raw = LR"tag(" /* raw */ \
int hidden;
)tag";
int afterRaw = 0b1010;
)fixture");

    for (const auto theme : {Theme::dark, Theme::light}) {
        applyTheme(app, theme);
        load(view, source);
        const auto colors = syntaxColors(view.palette());
        expectColor(view, source, "#include", colors.preprocessor);
        expectColor(view, source, "<dir//return.hpp>", colors.stringLiteral);
        expectColor(view, source, "/* leading */", colors.comment);
        expectColor(view, source, "#define", colors.preprocessor);
        expectColor(view, source, "1'000u", colors.number);
        expectColor(view, source, "+", colors.preprocessor);
        expectColor(view, source, "0x1.fp+2", colors.number);
        expectColor(view, source, "// macro comment", colors.comment);
        expectColor(view, source, "int printValue", colors.keyword, 3);
        expectColor(view, source, "printValue", view.palette().color(QPalette::Text));
        expectColor(view, source, "42", colors.number);
        expectColor(view, source, "u8\"https://example.com/return\"", colors.stringLiteral);
        expectColor(view, source, QString::fromUtf8("U'😀'"), colors.stringLiteral);
        expectColor(view, source, "LR\"tag(", colors.stringLiteral);
        expectColor(view, source, "/* raw */", colors.stringLiteral);
        expectColor(view, source, "int hidden;", colors.stringLiteral);
        expectColor(view, source, ")tag\"", colors.stringLiteral);
        expectColor(view, source, "int afterRaw", colors.keyword, 3);
        expectColor(view, source, "0b1010", colors.number);

        if (!output.isEmpty()) {
            const QString name = theme == Theme::dark ? "dark" : "light";
            require(view.grab().save(output + "/syntax-" + name + ".png"),
                    "Cannot save syntax preview");
        }
    }

    auto user = view.textCursor();
    user.setPosition(static_cast<int>(source.indexOf("printValue")));
    user.setPosition(user.position() + 10, QTextCursor::KeepAnchor);
    view.setTextCursor(user);
    QTextEdit::ExtraSelection entity;
    entity.cursor = user;
    entity.format.setBackground(Qt::yellow);
    view.setExtraSelections({entity});
    const auto textBefore = view.toPlainText();
    const auto linesBefore = view.sourceLineStarts();
    applyTheme(app, Theme::dark);
    QApplication::processEvents();
    require(view.toPlainText() == textBefore && view.sourceLineStarts() == linesBefore,
            "Changing theme must preserve source text and line coordinates");
    require(view.textCursor().selectedText() == "printValue"
                && view.extraSelections().size() == 1
                && view.extraSelections().front().cursor == entity.cursor,
            "Rehighlighting must preserve user selection and entity overlay");
    expectColor(view, source, "int afterRaw", syntaxColors(view.palette()).keyword, 3);
    view.setExtraSelections({});
}

void checkContinuations(CodeView& view) {
    const auto colors = syntaxColors(view.palette());
    for (int slashes = 1; slashes <= 4; ++slashes) {
        QString source = QStringLiteral("auto s = \"text")
            + QString(slashes, QLatin1Char('\\')) + QLatin1Char('\n');
        source += slashes % 2 == 0
            ? QStringLiteral("\"still\"; int after;\n")
            : QStringLiteral("\"; int after;\n");
        load(view, source);
        if (slashes % 2 == 0) expectColor(view, source, "still", colors.stringLiteral);
        expectColor(view, source, "int after", colors.keyword, 3);

        source = QStringLiteral("// comment") + QString(slashes, QLatin1Char('\\'))
            + QStringLiteral("\nint hidden;\nint visible;\n");
        load(view, source);
        expectColor(view, source, "int hidden;", colors.comment);
        expectColor(view, source, "int visible", colors.keyword, 3);
    }

    const auto broken = QStringLiteral("auto s = \"broken\nint recovered;\n");
    load(view, broken);
    expectColor(view, broken, "int recovered", colors.keyword, 3);

    const auto character = QStringLiteral("auto c = U'") + QLatin1Char('\\')
        + QStringLiteral("\nx'; int after;\n");
    load(view, character);
    expectColor(view, character, "x'", colors.stringLiteral);
    expectColor(view, character, "int after", colors.keyword, 3);

    const auto comment = QStringLiteral("/* comment *") + QLatin1Char('\\')
        + QStringLiteral("\n/ int after;\n");
    load(view, comment);
    expectColor(view, comment, "/ int after", colors.comment, 1);
    expectColor(view, comment, "int after", colors.keyword, 3);
}

void checkUnicodeAndRecovery(CodeView& view) {
    const auto colors = syntaxColors(view.palette());
    const auto source = QStringLiteral(
        "// comment\u2029int hidden;\u2028still hidden;\n"
        "int visible;\n"
        "auto s = \"left\u2029right\"; int afterQuote;\n"
        "auto raw = R\"custom(left\u2029int rawHidden;\n)custom\"; int afterRaw;\n"
        "/* leading */ # include /* between */ <dir//file.hpp>\n"
        "int value; # ordinary\n"
    );
    load(view, source);
    expectColor(view, source, "int hidden;", colors.comment);
    expectColor(view, source, "still hidden;", colors.comment);
    expectColor(view, source, "int visible", colors.keyword, 3);
    expectColor(view, source, "right\"", colors.stringLiteral);
    expectColor(view, source, "int afterQuote", colors.keyword, 3);
    expectColor(view, source, "int rawHidden;", colors.stringLiteral);
    expectColor(view, source, "int afterRaw", colors.keyword, 3);
    expectColor(view, source, "<dir//file.hpp>", colors.stringLiteral);
    expectColor(view, source, "# ordinary", view.palette().color(QPalette::Text));

    const auto directives = QStringLiteral("#") + QLatin1Char('\\')
        + QStringLiteral("\n include <other//file.hpp>\n")
        + QStringLiteral("#\u2029include <unicode//file.hpp>\n")
        + QStringLiteral("%:include <digraph//file.hpp>\n");
    load(view, directives);
    expectColor(view, directives, "<other//file.hpp>", colors.stringLiteral);
    expectColor(view, directives, "<unicode//file.hpp>", colors.stringLiteral);
    expectColor(view, directives, "<digraph//file.hpp>", colors.stringLiteral);

    const auto rawCases = QStringLiteral(
        "auto a = R\"(empty delimiter)\"; int afterEmpty;\n"
        "auto b = R\"abcdefghijklmnop(body\n)abcdefghijklmnop\"; int afterSixteen;\n"
        "auto c = R\"tag(body\\\n)tag\"; int afterSlash;\n"
        "auto d = \"escaped \\\" // still a string\"; int afterEscape;\n"
        "/*/ int notClosed;\n");
    load(view, rawCases);
    expectColor(view, rawCases, "int afterEmpty", colors.keyword, 3);
    expectColor(view, rawCases, "int afterSixteen", colors.keyword, 3);
    expectColor(view, rawCases, "int afterSlash", colors.keyword, 3);
    expectColor(view, rawCases, "// still a string", colors.stringLiteral);
    expectColor(view, rawCases, "int afterEscape", colors.keyword, 3);
    expectColor(view, rawCases, "int notClosed;", colors.comment);

    load(view, QStringLiteral("R\"open(unclosed\nint hidden;"));
    const auto nextFile = QStringLiteral("int recovered = .5e-2;\n");
    load(view, nextFile);
    expectColor(view, nextFile, "int recovered", colors.keyword, 3);
    expectColor(view, nextFile, ".5e-2", colors.number);
    load(view, {});
    require(view.toPlainText().isEmpty() && view.sourceLineStarts().size() == 1,
            "Clearing source must clear syntax and keep the empty-file line index valid");
}

void checkRawDelimiterChange(CodeView& view) {
    const auto source = QStringLiteral(
        "auto s = R\"one(\nbody\n)one\";\nint between;\n)two\";\nint after;\n");
    load(view, source);
    const auto colors = syntaxColors(view.palette());
    expectColor(view, source, "int between", colors.keyword, 3);
    QTextCursor edit(view.document());
    edit.setPosition(static_cast<int>(source.indexOf("one")));
    edit.setPosition(edit.position() + 3, QTextCursor::KeepAnchor);
    edit.insertText("two");
    QApplication::processEvents();
    expectColor(view, source, "int between;", colors.stringLiteral);
    expectColor(view, source, "int after", colors.keyword, 3);
}
} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    const QString output = app.arguments().value(1);
    try {
        if (!output.isEmpty()) require(QDir().mkpath(output), "Cannot create syntax output directory");
        applyTheme(app, Theme::dark);
        CodeView view;
        view.setAttribute(Qt::WA_DontShowOnScreen);
        view.setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        view.resize(900, 500);
        view.show();
        checkTokensAndTheme(app, view, output);
        checkContinuations(view);
        checkUnicodeAndRecovery(view);
        checkRawDelimiterChange(view);
        std::cout << "Syntax tokens, continuations, Unicode, themes and selections: passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
