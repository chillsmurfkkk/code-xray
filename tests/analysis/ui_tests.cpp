#include "ui_fixture.hpp"
#include "ui/shared/theme.hpp"
#include "ui/shared/popup_style.hpp"
#include "ui/shared/navigation.hpp"
#include "app/main_window.hpp"
#include "app/analysis_controller.hpp"
#include "code/api.hpp"
#include "ui/code/code_view.hpp"
#include "analysis/engine.hpp"
#include "ui/analysis/workspace.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QToolButton>
#include <QSettings>
#include <QThread>
#include <future>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>

namespace code = xray::code;
namespace ui = xray::ui::analysis_pages;
namespace a = xray::analysis;
namespace {
void populate(ui::Session& session) {
    auto input = analysis_test::fixture();
    session.setInput(input);
    session.present(a::analyze(input, session.profile(), {}));
}
template <class T> T* control(QWidget& workspace, const char* name) {
    auto* result = workspace.findChild<T*>(name);
    if (!result)
        qFatal("Missing UI control: %s", name);
    return result;
}
} // namespace

class AnalysisUiTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings_;
  private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("CodeXrayAnalysisTests");
        QCoreApplication::setApplicationName("Analysis");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_.path());
        xray::ui::installPopupStyle(*qApp);
        xray::ui::applyTheme(*qApp, xray::ui::Theme::dark);
    }
    void initialState() {
        ui::Session session;
        ui::Workspace workspace(session);
        QCOMPARE(control<QStackedWidget>(workspace, "analysisPages")->count(), 4);
        QCOMPARE(workspace.currentPage(), 1);
        QCOMPARE(control<QTableWidget>(workspace, "metricsTable")->rowCount(), 0);
        QVERIFY(!control<QPushButton>(workspace, "reanalyze")->isEnabled());
        QVERIFY(!control<QPushButton>(workspace, "applyRules")->isEnabled());
        QVERIFY(!control<QPushButton>(workspace, "navigateFinding")->isEnabled());
        QVERIFY(control<QLabel>(workspace, "analysisStatus")->text().contains("Немає результату"));
        auto empty = std::make_shared<code::ParsedSnapshot>(*analysis_test::fixture());
        empty->units.clear();
        session.setInput(empty);
        session.present(a::analyze(empty, session.profile(), {}));
        QVERIFY(control<QLabel>(workspace, "metricSummary")->text().contains("функцій немає"));
    }
    void profilesAndTransactionalImport() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ui::Session session;
        ui::Workspace workspace(session);
        QVERIFY(!session.createProfile("  "));
        QVERIFY(session.createProfile(QString::fromUtf8("Мій профіль")));
        const auto id = session.profile().id;
        QVERIFY(session.duplicateProfile("Copy"));
        QVERIFY(session.profile().id != id);
        const auto path = ui::filePath(directory.filePath(QString::fromUtf8("профіль.json")));
        QVERIFY(session.saveProfile(path));
        const auto copyId = session.profile().id;
        QVERIFY(session.importProfile(path));
        QVERIFY(session.profile().id != copyId);
        QCOMPARE(control<QListWidget>(workspace, "profilesList")->count(), 4);
        const auto savedId = session.profile().id;
        const auto bad = directory.filePath("broken.json");
        QFile file(bad);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{broken");
        file.close();
        QVERIFY(!session.importProfile(ui::filePath(bad)));
        QCOMPARE(session.profile().id, savedId);
        QCOMPARE(session.profiles().size(), std::size_t{4});
        QVERIFY(!session.saveProfile(ui::filePath(directory.filePath("missing/profile.json"))));
        control<QListWidget>(workspace, "profilesList")->setCurrentRow(0);
        QCOMPARE(session.selected(), 0);
    }
    void rulesValidationAndStaleness() {
        ui::Session session;
        ui::Workspace workspace(session);
        populate(session);
        QVERIFY(session.current());
        auto* threshold = control<QLineEdit>(workspace, "threshold0");
        threshold->setText("9223372036854775808");
        QVERIFY(!session.draftValid());
        QVERIFY(!session.current());
        QCOMPARE(session.profile().longFunctionLimit, std::int64_t{60});
        QVERIFY(!control<QPushButton>(workspace, "applyRules")->isEnabled());
        threshold->setText("-1");
        QVERIFY(!session.draftValid());
        threshold->setText("1.5");
        QVERIFY(!session.draftValid());
        threshold->setText("9223372036854775807");
        QVERIFY(session.draftValid());
        QCOMPARE(session.profile().longFunctionLimit, INT64_MAX);
        threshold->setText("60");
        QVERIFY(session.current());
        threshold->setText("61");
        QVERIFY(!session.current());
        QSignalSpy requested(&session, &ui::Session::analysisRequested);
        control<QPushButton>(workspace, "applyRules")->click();
        QCOMPARE(requested.count(), 1);
        session.present(a::analyze(session.snapshot()->parsed, session.profile(), {}));
        QVERIFY(session.current());
        QVERIFY(session.snapshot()->findings.empty()); // Equality is not a violation.
        auto* rules = control<QTableWidget>(workspace, "rulesTable");
        rules->item(0, 0)->setCheckState(Qt::Unchecked);
        QVERIFY(!session.profile().enabledRules[0]);
        QVERIFY(!session.current());
        threshold->setText("");
        QVERIFY(!session.draftValid());
        QVERIFY(session.createProfile("New"));
        QVERIFY(session.draftValid());
        QCOMPARE(threshold->text(), QString("60"));
    }
    void metricSortingFilteringAndDenominators() {
        ui::Session session;
        ui::Workspace workspace(session);
        populate(session);
        auto* table = control<QTableWidget>(workspace, "metricsTable");
        QCOMPARE(table->rowCount(), 5);
        QCOMPARE(table->item(0, 0)->text(), QString("fn0"));
        QCOMPARE(table->item(4, 2)->text(), QString::fromUtf8("—"));
        auto* summary = control<QLabel>(workspace, "metricSummary");
        QVERIFY(summary->text().contains("числових: 3"));
        QVERIFY(summary->text().contains("невідомих: 1"));
        QVERIFY(summary->text().contains("незастосовних: 1"));
        QVERIFY(summary->text().contains("лише синтаксично: 1"));
        control<QComboBox>(workspace, "aggregation")->setCurrentIndex(1);
        QVERIFY(summary->text().contains("61"));
        control<QComboBox>(workspace, "metricDirection")->setCurrentIndex(1);
        QCOMPARE(table->item(0, 0)->text(), QString("fn1"));
        control<QCheckBox>(workspace, "showUnknown")->setChecked(false);
        QCOMPARE(table->rowCount(), 3);
        control<QLineEdit>(workspace, "metricPath")->setText("include/");
        QCOMPARE(table->rowCount(), 0);
        QVERIFY(summary->text().contains("числових даних немає"));
        control<QCheckBox>(workspace, "showUnknown")->setChecked(true);
        QCOMPARE(table->rowCount(), 2);
        control<QLineEdit>(workspace, "metricPath")->clear();
        control<QComboBox>(workspace, "metricSort")->setCurrentIndex(1);
        QCOMPARE(table->item(0, 3)->text(), QString("0"));
        QVERIFY(control<QLabel>(workspace, "coverageStatus")->text().contains("часткове"));
    }
    void typedRequestsAndSnapshotNavigation() {
        ui::Session session;
        ui::Workspace workspace(session);
        populate(session);
        auto* table = control<QTableWidget>(workspace, "findingsTable");
        QCOMPARE(table->rowCount(), 1);
        table->selectRow(0);
        QSignalSpy navigation(&workspace, &ui::Workspace::codeActivated);
        control<QPushButton>(workspace, "navigateFinding")->click();
        QCOMPARE(navigation.count(), 1);
        QCOMPARE(navigation.at(0).at(0).value<std::shared_ptr<const a::AnalysisSnapshot>>(),
                 session.snapshot());
        QCOMPARE(navigation.at(0).at(1).toString(), QString("fn0"));
        QVERIFY(control<QPlainTextEdit>(workspace, "findingCode")->toPlainText().contains("fn0"));
        auto* codeView = control<QPlainTextEdit>(workspace, "findingCode");
        const auto displayedCode = codeView->toPlainText();
        session.reportMessage("Status updated");
        QCOMPARE(codeView->toPlainText(), displayedCode);
        QSignalSpy requested(&session, &ui::Session::analysisRequested);
        control<QPushButton>(workspace, "reanalyze")->click();
        QCOMPARE(requested.count(), 1);
        const auto copy = requested.at(0).at(0).value<a::AnalysisProfile>();
        session.editRules({true, true, true}, {100, 3, 10});
        QCOMPARE(copy.longFunctionLimit, std::int64_t{60});
        QVERIFY(!session.current());
        session.setBusy(true);
        session.requestAnalysis();
        QCOMPARE(requested.count(), 1);
        session.setBusy(false);
        auto previous = session.snapshot();
        session.present(xray::Cancelled{});
        QCOMPARE(session.snapshot(), previous);
        QVERIFY(session.message().contains("скасовано"));
        session.present(xray::Error{xray::ErrorCode::read_error, "failed", {}, {}, {}});
        QCOMPARE(session.snapshot(), previous);
        QVERIFY(session.message().contains("Помилка"));
        QCOMPARE(codeView->toPlainText(), displayedCode);
        QCOMPARE(table->currentRow(), 0);
        control<QLineEdit>(workspace, "findingSearch")->setText("absent");
        QCOMPARE(table->rowCount(), 0);
        QVERIFY(!control<QPushButton>(workspace, "navigateFinding")->isEnabled());
    }
    void hostThemeKeepsPageAndSelection() {
        ui::Session session;
        ui::Workspace workspace(session);
        populate(session);
        workspace.setCurrentPage(3);
        workspace.show();
        auto* table = control<QTableWidget>(workspace, "findingsTable");
        table->selectRow(0);
        control<QPushButton>(workspace, "navigateFinding")->click();
        auto* codeView = control<QPlainTextEdit>(workspace, "findingCode");
        const auto bytes = codeView->toPlainText();
        const auto position = codeView->textCursor().position();
        for (bool dark : {true, false, true}) {
            xray::ui::applyTheme(*qApp, dark ? xray::ui::Theme::dark : xray::ui::Theme::light);
            QCoreApplication::processEvents();
            QCOMPARE(workspace.currentPage(), 3);
            QCOMPARE(table->currentRow(), 0);
            QCOMPARE(codeView->toPlainText(), bytes);
            QCOMPARE(codeView->textCursor().position(), position);
            QVERIFY(!codeView->extraSelections().empty());
            QCOMPARE(codeView->extraSelections()[0].format.background(), QApplication::palette().highlight());
        }
    }

    void backgroundAnalysisAndInputReplacement() {
        xray::app::AnalysisController controller;
        auto& session = controller.session();
        auto first = analysis_test::fixture();
        controller.setInput(first);
        QVERIFY(session.busy());
        session.editRules({true, true, true}, {100, 3, 10});
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 10000);
        QVERIFY(session.snapshot());
        QCOMPARE(session.snapshot()->profile.longFunctionLimit, std::int64_t{60});
        QVERIFY(!session.current());
        session.requestAnalysis();
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 10000);
        QVERIFY(session.current());
        QVERIFY(session.snapshot()->findings.empty());
        const auto previous = session.snapshot();
        session.requestAnalysis();
        controller.cancel();
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 10000);
        QCOMPARE(session.snapshot(), previous);
        QVERIFY(session.message().contains("скасовано"));
        controller.setInput(first);
        auto replacement = std::make_shared<code::ParsedSnapshot>(*first);
        replacement->units.clear();
        controller.setInput(replacement);
        QVERIFY(!session.current());
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 10000);
        QCOMPARE(session.snapshot()->parsed, std::shared_ptr<const code::ParsedSnapshot>(replacement));
        QVERIFY(session.current());
        QVERIFY(session.snapshot()->metrics.empty());
        controller.setInput(nullptr);
        QVERIFY(!session.current());
        QVERIFY(!session.hasInput());
    }
    void integratedWindowWithRealSource() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile file(directory.filePath("sample.cpp"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("int function(int x) {\n    if(x) ++x;\n    return x;\n}\n");
        file.close();
        const auto root = ui::filePath(directory.path());
        auto loaded = std::async(std::launch::async, [root] {
                          return code::collectAndParse({root, {}}, {}, {});
                      }).get();
        QVERIFY(std::holds_alternative<code::ParsedSnapshot>(loaded));
        auto parsed =
            std::make_shared<const code::ParsedSnapshot>(std::move(std::get<code::ParsedSnapshot>(loaded)));
        xray::app::MainWindow window(xray::ui::Theme::dark);
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.show();
        window.setSnapshot(parsed);
        auto* controller = window.findChild<xray::app::AnalysisController*>();
        QVERIFY(controller);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->session().busy(), 10000);
        QVERIFY(controller->session().current());
        QCOMPARE(controller->session().snapshot()->parsed, parsed);
        auto* navigation = window.findChild<xray::ui::NavigationPanel*>();
        QVERIFY(navigation);
        auto* workspace = window.findChild<ui::Workspace*>();
        QVERIFY(workspace);
        auto* button = window.findChild<QToolButton*>("analysisSection");
        QVERIFY(button && button->isEnabled());
        button->click();
        QCOMPARE(workspace->currentPage(), 1);
        QVERIFY(workspace->isVisible());
        for (int page = 0; page < 4; ++page) {
            navigation->setCurrentPage(xray::ui::Section::analysis, page);
            QCOMPARE(workspace->currentPage(), page);
            QVERIFY(workspace->isVisible());
        }
        control<QLineEdit>(*workspace, "threshold0")->setText("0");
        control<QPushButton>(*workspace, "applyRules")->click();
        QTRY_VERIFY_WITH_TIMEOUT(!controller->session().busy(), 10000);
        auto* findings = control<QTableWidget>(*workspace, "findingsTable");
        QCOMPARE(findings->rowCount(), 1);
        findings->selectRow(0);
        control<QPushButton>(*workspace, "navigateFinding")->click();
        auto* view = workspace->findChild<xray::ui::CodeView*>();
        QVERIFY(view);
        QCOMPARE(view->toPlainText(), QString::fromUtf8(*parsed->source->files[0].bytes));
        QVERIFY(!view->extraSelections().isEmpty());
        navigation->setCurrentPage(xray::ui::Section::review, 0);
        QVERIFY(!workspace->isVisible());
        navigation->setCurrentPage(xray::ui::Section::analysis, 3);
        QVERIFY(workspace->isVisible());
        QCOMPARE(findings->currentRow(), 0);
        window.findChild<QToolButton*>("themeToggle")->click();
        QCoreApplication::processEvents();
        QCOMPARE(workspace->currentPage(), 3);
        QCOMPARE(view->toPlainText(), QString::fromUtf8(*parsed->source->files[0].bytes));
        window.findChild<QToolButton*>("pagesToggle")->click();
        QCOMPARE(workspace->currentPage(), 3); // Collapsing navigation preserves analysis state.
        navigation->setCurrentPage(xray::ui::Section::code, 2);
        QVERIFY(!workspace->isVisible());
    }
    void visualDemo() {
        const auto output = qEnvironmentVariable("CODE_XRAY_GUI_CAPTURE_DIR");
        if (output.isEmpty())
            QSKIP("Set CODE_XRAY_GUI_CAPTURE_DIR for visual inspection captures");
        xray::app::MainWindow window(xray::ui::Theme::dark);
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(1180, 820);
        window.show();
        window.setSnapshot(analysis_test::fixture(), true);
        auto* controller = window.findChild<xray::app::AnalysisController*>();
        QVERIFY(controller);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->session().busy(), 10000);
        auto* workspace = window.findChild<ui::Workspace*>();
        QVERIFY(workspace);
        auto* navigation = window.findChild<xray::ui::NavigationPanel*>();
        QVERIFY(navigation);
        QVERIFY(QDir().mkpath(output));
        for (bool dark : {true, false}) {
            xray::ui::applyTheme(*qApp, dark ? xray::ui::Theme::dark : xray::ui::Theme::light);
            for (int i = 0; i < 4; ++i) {
                navigation->setCurrentPage(xray::ui::Section::analysis, i);
                if (i == 3) {
                    control<QTableWidget>(*workspace, "findingsTable")->selectRow(0);
                    control<QPushButton>(*workspace, "navigateFinding")->click();
                }
                QCoreApplication::processEvents();
                QVERIFY(window.grab().save(
                    output + QString("/%1-analysis-%2.png").arg(dark ? "dark" : "light").arg(i)));
            }
        }
        window.resize(800, 540);
        QCoreApplication::processEvents();
        QVERIFY(window.grab().save(output + "/analysis-narrow.png"));
    }
};
QTEST_MAIN(AnalysisUiTests)
#include "ui_tests.moc"
