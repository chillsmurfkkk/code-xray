#include <QTest>
#include <QSignalSpy>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>

#include "ui/history/CommitsPage.hpp"
#include "ui/history/HistoryFiltersPage.hpp"
#include "ui/history/CodeChurnPage.hpp"
#include "ui/history/AncestryPage.hpp"
#include "ui/history/history_fixtures.hpp"
#include "app/main_window.hpp"
#include "ui/shared/navigation.hpp"
#include "ui/shared/theme.hpp"

namespace {
template <typename T>
T* findControl(QWidget* parent, const QString& name) {
    auto* ctrl = parent->findChild<T*>(name);
    if (!ctrl) {
        qFatal("Control not found: %s", qPrintable(name));
    }
    return ctrl;
}
} // namespace

class HistoryUiTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("CodeXrayHistoryTests");
        QCoreApplication::setApplicationName("HistoryTests");
        xray::ui::applyTheme(*qApp, xray::ui::Theme::dark);
    }

    void testCommitsPage() {
        xray::ui::history::CommitsPage page;
        page.resize(800, 600);

        auto* demoBadge = findControl<QLabel>(&page, "commitsDemoBadge");
        QVERIFY(!demoBadge->isHidden());

        auto* table = findControl<QTableWidget>(&page, "commitsTable");
        QCOMPARE(table->rowCount(), 6);

        // Test search filter
        auto* searchEdit = findControl<QLineEdit>(&page, "commitSearchEdit");
        searchEdit->setText("Petrenko");
        QCOMPARE(table->rowCount(), 2);

        searchEdit->clear();
        QCOMPARE(table->rowCount(), 6);

        // Test LastN selection mode
        auto* combo = findControl<QComboBox>(&page, "commitSelectionCombo");
        auto* spin = findControl<QSpinBox>(&page, "commitLimitSpin");
        spin->setValue(3);
        combo->setCurrentIndex(1); // LastN
        QCOMPARE(table->rowCount(), 3);

        // Test periodic selection mode (step 2)
        combo->setCurrentIndex(2); // Periodic
        QCOMPARE(table->rowCount(), 3);

        // Reset to all
        combo->setCurrentIndex(0);
        QCOMPARE(table->rowCount(), 6);

        // Test sort toggle
        auto* sortBtn = findControl<QPushButton>(&page, "commitSortBtn");
        sortBtn->click();
        QCOMPARE(table->rowCount(), 6);

        // Test keyboard / current item change updating details (P2 fix)
        auto* details = findControl<QLabel>(&page, "commitDetailsLabel");
        table->setCurrentCell(2, 0);
        QVERIFY(details->text().contains(table->item(2, 0)->text()));

        // Test HTML escaping in details label (P2 fix)
        xray::history::CommitRecord htmlCommit{
            "html123", {}, "<Special & Author>", "special@test.com",
            std::chrono::sys_seconds{}, std::chrono::sys_seconds{},
            "Fix <type> & parsing issues <script>"
        };
        page.setCommits({ htmlCommit }, false);
        QCOMPARE(table->rowCount(), 1);
        QVERIFY(demoBadge->isHidden());
        table->setCurrentCell(0, 0);
        QVERIFY(details->text().contains("&lt;Special &amp; Author&gt;"));
        QVERIFY(details->text().contains("&lt;type&gt; &amp; parsing"));
        QVERIFY(!details->text().contains("<script>"));
    }

    void testHistoryFiltersPage() {
        xray::ui::history::HistoryFiltersPage page;
        page.resize(800, 600);

        auto* demoBadge = findControl<QLabel>(&page, "filterDemoBadge");
        QVERIFY(!demoBadge->isHidden());

        auto* table = findControl<QTableWidget>(&page, "filterResultsTable");
        QCOMPARE(table->rowCount(), 6);

        // Test AuthorHistoryQuery
        auto* authorEdit = findControl<QLineEdit>(&page, "filterAuthorEdit");
        auto* applyBtn = findControl<QPushButton>(&page, "filterApplyBtn");
        auto* resetBtn = findControl<QPushButton>(&page, "filterResetBtn");
        auto* statusLabel = findControl<QLabel>(&page, "filterStatusLabel");

        authorEdit->setText("Oleksandr Kostiuk");
        applyBtn->click();
        QCOMPARE(table->rowCount(), 2);
        QVERIFY(statusLabel->styleSheet().contains("#b8bb26"));

        // Test dynamic theme switch contrast update
        xray::ui::applyTheme(*qApp, xray::ui::Theme::light);
        qApp->processEvents();
        QVERIFY(statusLabel->styleSheet().contains("#427b58"));
        xray::ui::applyTheme(*qApp, xray::ui::Theme::dark);
        qApp->processEvents();

        // Test reset
        resetBtn->click();
        QCOMPARE(table->rowCount(), 6);

        // Test MergeCommitQuery
        auto* mergeCheck = findControl<QCheckBox>(&page, "filterOnlyMergesCheck");
        mergeCheck->setChecked(true);
        applyBtn->click();
        QCOMPARE(table->rowCount(), 1);

        resetBtn->click();
        QCOMPARE(table->rowCount(), 6);

        // Test setCommits with false demo badge
        page.setCommits(xray::ui::history::makeDemoCommits(), false);
        QVERIFY(demoBadge->isHidden());
    }

    void testCodeChurnPage() {
        xray::ui::history::CodeChurnPage page;
        page.resize(800, 600);

        auto* demoBadge = findControl<QLabel>(&page, "churnDemoBadge");
        QVERIFY(!demoBadge->isHidden());

        auto* table = findControl<QTableWidget>(&page, "churnChangesTable");
        QCOMPARE(table->rowCount(), 8);

        auto* addedLabel = findControl<QLabel>(&page, "churnAddedLabel");
        auto* deletedLabel = findControl<QLabel>(&page, "churnDeletedLabel");
        auto* completeness = findControl<QLabel>(&page, "churnCompletenessBadge");

        // Initial churn check
        QVERIFY(addedLabel->text().contains("+"));
        QVERIFY(deletedLabel->text().contains("-"));
        QVERIFY(addedLabel->styleSheet().contains("#b8bb26"));

        // Test dynamic theme switch updates churn KPI colors
        xray::ui::applyTheme(*qApp, xray::ui::Theme::light);
        qApp->processEvents();
        QVERIFY(addedLabel->styleSheet().contains("#427b58"));
        xray::ui::applyTheme(*qApp, xray::ui::Theme::dark);
        qApp->processEvents();

        // One record has nullopt lines (assets/logo.png), so hasUnknownLineCounts must be set
        QVERIFY(completeness->text().contains("невідомі"));

        // Test searching renamed file by old path (P2 fix)
        auto* pathEdit = findControl<QLineEdit>(&page, "churnPathEdit");
        pathEdit->setText("spec_draft.md");
        QCOMPARE(table->rowCount(), 1);
        QVERIFY(table->item(0, 1)->text().contains("spec_draft.md → docs/spec_v1.md"));

        // Reset
        auto* resetBtn = findControl<QPushButton>(&page, "churnResetBtn");
        resetBtn->click();
        QCOMPARE(table->rowCount(), 8);
    }

    void testAncestryPage() {
        xray::ui::history::AncestryPage page;
        page.resize(800, 600);

        auto* demoBadge = findControl<QLabel>(&page, "ancestryDemoBadge");
        QVERIFY(!demoBadge->isHidden());

        auto* ancestorEdit = findControl<QLineEdit>(&page, "ancestorOidEdit");
        auto* descendantEdit = findControl<QLineEdit>(&page, "descendantOidEdit");
        auto* checkBtn = findControl<QPushButton>(&page, "checkAncestryBtn");
        auto* resultLabel = findControl<QLabel>(&page, "ancestryResultLabel");

        // Test valid ancestor: d1a5090 is root ancestor of c6f81a2
        ancestorEdit->setText("d1a5090");
        descendantEdit->setText("c6f81a2");
        checkBtn->click();
        QVERIFY(resultLabel->text().contains("Підтверджено"));

        // Test dynamic theme switch updates ancestry label color
        xray::ui::applyTheme(*qApp, xray::ui::Theme::light);
        qApp->processEvents();
        QVERIFY(resultLabel->styleSheet().contains("#427b58"));
        xray::ui::applyTheme(*qApp, xray::ui::Theme::dark);
        qApp->processEvents();
        QVERIFY(resultLabel->styleSheet().contains("#b8bb26"));

        // Test non-ancestor: c6f81a2 is NOT ancestor of d1a5090
        ancestorEdit->setText("c6f81a2");
        descendantEdit->setText("d1a5090");
        checkBtn->click();
        QVERIFY(resultLabel->text().contains("Відхилено"));

        // Test LinearHistorySelection tracing
        auto* startEdit = findControl<QLineEdit>(&page, "linearStartEdit");
        auto* traceBtn = findControl<QPushButton>(&page, "traceLinearBtn");
        auto* linearTable = findControl<QTableWidget>(&page, "linearHistoryTable");

        startEdit->setText("c6f81a2");
        traceBtn->click();
        QVERIFY(linearTable->rowCount() >= 3);

        // Test replacing history with another non-empty set resets ancestry result label (P2 fix)
        xray::history::CommitRecord c1{"new1", {"new2"}, "Author A", "a@test.com", {}, {}, "Msg 1"};
        xray::history::CommitRecord c2{"new2", {}, "Author B", "b@test.com", {}, {}, "Msg 2"};
        page.setCommits({c1, c2}, false);
        QVERIFY(!resultLabel->text().contains("Підтверджено"));
        QVERIFY(!resultLabel->text().contains("Відхилено"));
        QVERIFY(resultLabel->text().contains("Введіть OID"));
        QCOMPARE(ancestorEdit->text(), QString("new2"));
        QCOMPARE(descendantEdit->text(), QString("new1"));
        QVERIFY(demoBadge->isHidden());

        // Test empty commits clears previous results (P2 fix)
        page.setCommits({});
        QVERIFY(ancestorEdit->text().isEmpty());
        QVERIFY(descendantEdit->text().isEmpty());
        QVERIFY(startEdit->text().isEmpty());
        QCOMPARE(linearTable->rowCount(), 0);
        QVERIFY(resultLabel->text().contains("порожня"));
    }

    void testMainWindowIntegrationAndThemes() {
        xray::app::MainWindow window(xray::ui::Theme::dark);
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(1180, 760);
        window.show();
        qApp->processEvents();

        auto* nav = window.findChild<xray::ui::NavigationPanel*>();
        QVERIFY(nav != nullptr);

        // Navigate to History section, page 0 (Commits)
        nav->setCurrentPage(xray::ui::Section::history, 0);
        qApp->processEvents();

        auto* commitsTable = window.findChild<QTableWidget*>("commitsTable");
        QVERIFY(commitsTable != nullptr);
        QVERIFY(commitsTable->isVisible());

        // Test light theme switch (P2 fix for contrast & readability)
        xray::ui::applyTheme(*qApp, xray::ui::Theme::light);
        qApp->processEvents();

        nav->setCurrentPage(xray::ui::Section::history, 1);
        qApp->processEvents();
        auto* filterTable = window.findChild<QTableWidget*>("filterResultsTable");
        QVERIFY(filterTable != nullptr);
        QVERIFY(filterTable->isVisible());

        nav->setCurrentPage(xray::ui::Section::history, 2);
        qApp->processEvents();
        auto* churnTable = window.findChild<QTableWidget*>("churnChangesTable");
        QVERIFY(churnTable != nullptr);
        QVERIFY(churnTable->isVisible());

        nav->setCurrentPage(xray::ui::Section::history, 3);
        qApp->processEvents();
        auto* linearTable = window.findChild<QTableWidget*>("linearHistoryTable");
        QVERIFY(linearTable != nullptr);
        QVERIFY(linearTable->isVisible());

        // Restore dark theme
        xray::ui::applyTheme(*qApp, xray::ui::Theme::dark);
        qApp->processEvents();

        window.close();
    }
};

QTEST_MAIN(HistoryUiTests)
#include "ui_tests.moc"
