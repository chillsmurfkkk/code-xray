#include <QTest>
#include <QSignalSpy>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QTemporaryDir>
#include <QFile>

#include "ui/review/ComparisonPage.hpp"
#include "ui/review/TrendPage.hpp"
#include "ui/review/ExportPage.hpp"
#include "ui/review/ChangeCardPage.hpp"
#include "ui/history/CodeChurnPage.hpp"
#include "ui/review/review_fixtures.hpp"
#include "app/main_window.hpp"
#include "app/coordinator.hpp"
#include "ui/shared/navigation.hpp"
#include "ui/shared/theme.hpp"

#include <nlohmann/json.hpp>

namespace {
template <typename T>
T* findControl(QWidget* parent, const QString& name) {
    auto* ctrl = parent->findChild<T*>(name);
    if (!ctrl) {
        qFatal("Control not found: %s", qPrintable(name));
    }
    return ctrl;
}

xray::review::ComparisonReport makeRealComparisonReport() {
    auto rep = xray::ui::review::makeDemoComparisonReport();
    rep.baseId = "analysis_fingerprint_base";
    rep.targetId = "analysis_fingerprint_target";
    if (rep.base && rep.base->parsed && rep.base->parsed->source) {
        const_cast<xray::code::SourceSnapshot*>(rep.base->parsed->source.get())->commitOid = "git_oid_base_111";
    }
    if (rep.target && rep.target->parsed && rep.target->parsed->source) {
        const_cast<xray::code::SourceSnapshot*>(rep.target->parsed->source.get())->commitOid = "git_oid_target_222";
    }
    return rep;
}

xray::review::TrendReport makeRealTrendReport() {
    auto rep = xray::ui::review::makeDemoTrendReport();
    rep.inputs.chainTipOid = "commit_target_002";
    rep.options.entityKey = "multiply(int,int)";
    return rep;
}
} // namespace

class ReviewUiTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("CodeXrayReviewTests");
        QCoreApplication::setApplicationName("ReviewTests");
        xray::ui::applyTheme(*qApp, xray::ui::Theme::dark);
    }

    // 1. Реальний звіт не замінюється демо при натисканні «Порівняти»
    void testComparisonPage_RealReportNotReplacedByDemo() {
        xray::ui::review::ComparisonPage page;
        page.resize(900, 600);

        auto* demoBadge = findControl<QLabel>(&page, "comparisonDemoBadge");
        QVERIFY(!demoBadge->isHidden());
        QVERIFY(page.isDemo());

        // Завантажуємо реальний звіт
        auto realReport = makeRealComparisonReport();
        page.setReport(realReport, /*isDemo=*/false, "git_oid_base_111", "git_oid_target_222");

        QVERIFY(demoBadge->isHidden());
        QVERIFY(!page.isDemo());
        QCOMPARE(page.currentReport().baseId, std::string("analysis_fingerprint_base"));
        QCOMPARE(page.currentReport().targetId, std::string("analysis_fingerprint_target"));
        QCOMPARE(page.baseOid(), QString("git_oid_base_111"));
        QCOMPARE(page.targetOid(), QString("git_oid_target_222"));

        // Перевіряємо сигнал запиту перебудови реального звіту
        QSignalSpy rebuildSpy(&page, &xray::ui::review::ComparisonPage::compareRebuildRequested);
        QSignalSpy compareSpy(&page, &xray::ui::review::ComparisonPage::compareRequested);

        auto* compareBtn = findControl<QPushButton>(&page, "comparisonCompareButton");
        compareBtn->click();

        // Реальний звіт НЕ замінено на демо!
        QVERIFY(!page.isDemo());
        QVERIFY(demoBadge->isHidden());
        QCOMPARE(page.currentReport().baseId, std::string("analysis_fingerprint_base"));
        QCOMPARE(page.currentReport().targetId, std::string("analysis_fingerprint_target"));

        // Сигнал compareRebuildRequested емітовано з Git commitOid, а не analysis fingerprint!
        QCOMPARE(rebuildSpy.count(), 1);
        auto args = rebuildSpy.takeFirst();
        QCOMPARE(args.at(0).toString(), QString("git_oid_base_111"));
        QCOMPARE(args.at(1).toString(), QString("git_oid_target_222"));
        QCOMPARE(compareSpy.count(), 1);

        // Перевірка фільтрації та вибору елементів
        auto* table = findControl<QTableWidget>(&page, "comparisonReviewTable");
        QVERIFY(table->rowCount() > 0);
        auto* kindCombo = findControl<QComboBox>(&page, "comparisonKindFilter");
        kindCombo->setCurrentIndex(1); // "Змінено"
        QVERIFY(table->rowCount() >= 1);
        kindCombo->setCurrentIndex(0); // "Усі"
    }

    // 2. Обрана функція на сторінці тренду дає реальний тренд
    void testTrendPage_SelectedFunctionBuildsTrend() {
        xray::ui::review::TrendPage page;
        page.resize(900, 600);

        auto* demoBadge = findControl<QLabel>(&page, "trendDemoBadge");
        QVERIFY(!demoBadge->isHidden());

        // Задаємо OID вершини репозиторію та список функцій
        page.setRepositoryTip("commit_tip_abc");
        page.setAvailableTargets({"computeHash(void*)", "renderFrame()"}, {"src/crypto.cpp", "src/view.cpp"});

        auto* entityCombo = findControl<QComboBox>(&page, "trendEntityCombo");
        QVERIFY(entityCombo->count() >= 2);
        entityCombo->setCurrentIndex(0);
        QString selectedEntity = entityCombo->currentData().toString();
        QCOMPARE(selectedEntity, QString("computeHash(void*)"));

        QSignalSpy rebuildSpy(&page, &xray::ui::review::TrendPage::trendRebuildRequested);

        auto* buildBtn = findControl<QPushButton>(&page, "trendBuildButton");
        buildBtn->click();

        QCOMPARE(rebuildSpy.count(), 1);
        auto args = rebuildSpy.takeFirst();
        QCOMPARE(args.at(0).toString(), QString("commit_tip_abc"));
        QCOMPARE(args.at(3).toString(), QString("computeHash(void*)"));

        // Перевіряємо відображення отриманого реального тренду
        auto realTrend = makeRealTrendReport();
        page.setTrendReport(realTrend, /*isDemo=*/false);
        QVERIFY(demoBadge->isHidden());

        auto* pointsTable = findControl<QTableWidget>(&page, "trendPointsTable");
        QCOMPARE(pointsTable->rowCount(), static_cast<int>(realTrend.points.size()));
        QVERIFY(pointsTable->rowCount() > 0);
    }

    // 3. Реальні HTML та JSON файли створюються, записуються й валідуються
    void testExportPage_HtmlJsonCreationAndReadback() {
        xray::ui::review::ExportPage page;
        page.resize(900, 600);

        auto* demoBadge = findControl<QLabel>(&page, "exportDemoBadge");
        QVERIFY(!demoBadge->isHidden());

        auto realReport = makeRealComparisonReport();
        page.setReport(realReport, /*isDemo=*/false);
        QVERIFY(demoBadge->isHidden());

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto* formatCombo = findControl<QComboBox>(&page, "exportFormatCombo");
        auto* pathEdit = findControl<QLineEdit>(&page, "exportPathEdit");
        auto* saveBtn = findControl<QPushButton>(&page, "exportSaveButton");
        auto* previewBtn = findControl<QPushButton>(&page, "exportPreviewButton");
        auto* previewArea = findControl<QTextEdit>(&page, "exportPreviewArea");
        auto* statusLabel = findControl<QLabel>(&page, "exportStatusLabel");

        // --- Експорт у JSON ---
        const int jsonIdx = formatCombo->findData(static_cast<int>(xray::review::ExportFormat::json));
        formatCombo->setCurrentIndex(jsonIdx >= 0 ? jsonIdx : 1);
        QString jsonPath = tempDir.filePath("test_report.json");
        pathEdit->setText(jsonPath);

        // Перевірка попереднього перегляду JSON (асинхронно)
        QSignalSpy jsonPreviewSpy(&page, &xray::ui::review::ExportPage::previewCompleted);
        previewBtn->click();
        QVERIFY(jsonPreviewSpy.wait(5000));
        QVERIFY(!previewArea->toPlainText().isEmpty());
        auto previewJson = nlohmann::json::parse(previewArea->toPlainText().toStdString(), nullptr, false);
        QVERIFY(!previewJson.is_discarded());

        // Збереження файлу (у фоновому потоці)
        QSignalSpy jsonCompletedSpy(&page, &xray::ui::review::ExportPage::exportCompleted);
        saveBtn->click();
        QVERIFY(jsonCompletedSpy.wait(5000));
        QVERIFY(QFile::exists(jsonPath));
        QVERIFY(statusLabel->text().contains(QString::fromUtf8("Успішно збережено")));

        // Читання та верифікація згенерованого JSON
        QFile jsonFile(jsonPath);
        QVERIFY(jsonFile.open(QIODevice::ReadOnly));
        QByteArray jsonBytes = jsonFile.readAll();
        jsonFile.close();

        auto parsed = nlohmann::json::parse(jsonBytes.toStdString(), nullptr, false);
        QVERIFY(!parsed.is_discarded());
        QVERIFY(parsed.contains("baseId"));
        QVERIFY(parsed.contains("targetId"));
        QVERIFY(parsed.contains("items"));
        QCOMPARE(parsed["baseId"].get<std::string>(), std::string("analysis_fingerprint_base"));
        QCOMPARE(parsed["targetId"].get<std::string>(), std::string("analysis_fingerprint_target"));

        // Прев'ю та збережений JSON мають ідентичні дані (coverage, items, summary)
        QCOMPARE(parsed["items"].size(), previewJson["items"].size());
        QCOMPARE(parsed["summary"].get<std::string>(), previewJson["summary"].get<std::string>());

        // --- Експорт у HTML ---
        const int htmlIdx = formatCombo->findData(static_cast<int>(xray::review::ExportFormat::html));
        formatCombo->setCurrentIndex(htmlIdx >= 0 ? htmlIdx : 0);
        QString htmlPath = tempDir.filePath("test_report.html");
        pathEdit->setText(htmlPath);

        // Перевірка попереднього перегляду HTML (асинхронно)
        QSignalSpy htmlPreviewSpy(&page, &xray::ui::review::ExportPage::previewCompleted);
        previewBtn->click();
        QVERIFY(htmlPreviewSpy.wait(5000));
        QString previewHtml = previewArea->toPlainText();
        QVERIFY(previewHtml.contains("<html") || previewHtml.contains("<!DOCTYPE"));

        // Збереження файлу (у фоновому потоці)
        QSignalSpy htmlCompletedSpy(&page, &xray::ui::review::ExportPage::exportCompleted);
        saveBtn->click();
        QVERIFY(htmlCompletedSpy.wait(5000));
        QVERIFY(QFile::exists(htmlPath));
        QVERIFY(statusLabel->text().contains(QString::fromUtf8("Успішно збережено")));

        // Читання та верифікація згенерованого HTML
        QFile htmlFile(htmlPath);
        QVERIFY(htmlFile.open(QIODevice::ReadOnly | QIODevice::Text));
        QString htmlContent = QString::fromUtf8(htmlFile.readAll());
        htmlFile.close();

        QVERIFY(htmlContent.contains("<html", Qt::CaseInsensitive));
        QVERIFY(htmlContent.contains("analysis_fingerprint_base"));
        QVERIFY(htmlContent.contains("analysis_fingerprint_target"));
        // Прев'ю та файл згенеровані через спільну серіалізацію
        QCOMPARE(previewHtml, htmlContent);
    }

    // 4. Помилка та скасування коректно обробляються й показуються в UI
    void testErrorAndCancellationHandling() {
        // TrendPage: помилка при спробі побудови без вказання tipOid
        {
            xray::ui::review::TrendPage trendPage;
            trendPage.setRepositoryTip(QString{}); // Порожній OID
            auto* buildBtn = findControl<QPushButton>(&trendPage, "trendBuildButton");
            auto* statusLabel = findControl<QLabel>(&trendPage, "trendStatusLabel");

            buildBtn->click();
            QVERIFY(statusLabel->text().contains(QString::fromUtf8("Спочатку відкрийте Git-репозиторій")));

            // Тест встановлення повідомлень про помилку та скасування
            trendPage.setStatusText(QString::fromUtf8("Помилка: не вдалося завантажити коміт"));
            QVERIFY(trendPage.statusText().contains(QString::fromUtf8("Помилка")));

            trendPage.setStatusText(QString::fromUtf8("Операцію скасовано"));
            QVERIFY(trendPage.statusText().contains(QString::fromUtf8("скасовано")));
        }

        // ExportPage: помилка при невалідному шляху
        {
            xray::ui::review::ExportPage exportPage;
            auto* pathEdit = findControl<QLineEdit>(&exportPage, "exportPathEdit");
            auto* statusLabel = findControl<QLabel>(&exportPage, "exportStatusLabel");

            pathEdit->setText(""); // Порожній шлях
            auto* saveBtn = findControl<QPushButton>(&exportPage, "exportSaveButton");
            saveBtn->click();
            QVERIFY(statusLabel->text().contains(QString::fromUtf8("Вкажіть шлях")));
        }

        // ExportPage: асинхронне скасування експорту
        {
            xray::ui::review::ExportPage exportPage;
            auto realReport = makeRealComparisonReport();
            exportPage.setReport(realReport, /*isDemo=*/false);

            QTemporaryDir tempDir;
            QVERIFY(tempDir.isValid());
            auto* pathEdit = findControl<QLineEdit>(&exportPage, "exportPathEdit");
            auto* saveBtn = findControl<QPushButton>(&exportPage, "exportSaveButton");
            auto* cancelBtn = findControl<QPushButton>(&exportPage, "exportCancelButton");
            auto* statusLabel = findControl<QLabel>(&exportPage, "exportStatusLabel");

            pathEdit->setText(tempDir.filePath("cancelled.json"));

            QSignalSpy cancelledSpy(&exportPage, &xray::ui::review::ExportPage::exportCancelled);
            saveBtn->click();
            cancelBtn->click();
            QVERIFY(cancelledSpy.wait(5000));
            QVERIFY(!exportPage.isExporting());
            QVERIFY(statusLabel->text().contains(QString::fromUtf8("скасовано")));
        }

        // ExportPage: асинхронне скасування прев'ю
        {
            xray::ui::review::ExportPage exportPage;
            auto realReport = makeRealComparisonReport();
            exportPage.setReport(realReport, /*isDemo=*/false);

            auto* previewBtn = findControl<QPushButton>(&exportPage, "exportPreviewButton");
            auto* cancelBtn = findControl<QPushButton>(&exportPage, "exportCancelButton");
            auto* statusLabel = findControl<QLabel>(&exportPage, "exportStatusLabel");

            QSignalSpy cancelledSpy(&exportPage, &xray::ui::review::ExportPage::previewCancelled);
            previewBtn->click();
            cancelBtn->click();
            QVERIFY(cancelledSpy.wait(5000));
            QVERIFY(!exportPage.isPreviewing());
            QVERIFY(statusLabel->text().contains(QString::fromUtf8("скасовано")));
        }

        // ExportPage: [P2] заміна звіту відкидає старе прев'ю
        {
            xray::ui::review::ExportPage exportPage;
            auto report1 = makeRealComparisonReport();
            exportPage.setReport(report1, /*isDemo=*/false);

            auto* previewBtn = findControl<QPushButton>(&exportPage, "exportPreviewButton");
            auto* previewArea = findControl<QTextEdit>(&exportPage, "exportPreviewArea");

            previewBtn->click();
            // Встановлюємо новий звіт під час або після запиту прев'ю
            auto report2 = makeRealComparisonReport();
            report2.targetId = "second_report_target_oid";
            exportPage.setReport(report2, /*isDemo=*/false);

            QVERIFY(previewArea->toPlainText().isEmpty());
            QVERIFY(!exportPage.isPreviewing());
        }

        // ExportPage: [P2] якщо файл уже записався, але надійшов новий звіт, результат експорту не відкидається
        {
            xray::ui::review::ExportPage exportPage;
            auto report1 = makeRealComparisonReport();
            exportPage.setReport(report1, /*isDemo=*/false);

            QTemporaryDir tempDir;
            QVERIFY(tempDir.isValid());
            auto exportFile = tempDir.filePath("report1_exported.json");

            auto* pathEdit = findControl<QLineEdit>(&exportPage, "exportPathEdit");
            pathEdit->setText(exportFile);

            auto* exportBtn = findControl<QPushButton>(&exportPage, "exportSaveButton");
            auto* statusLabel = findControl<QLabel>(&exportPage, "exportStatusLabel");

            QSignalSpy completedSpy(&exportPage, &xray::ui::review::ExportPage::exportCompleted);

            // Запускаємо експорт першого звіту
            exportBtn->click();

            // Негайно передаємо новий звіт на сторінку (до або під час завершення запису)
            auto report2 = makeRealComparisonReport();
            report2.targetId = "second_report_target_oid";
            exportPage.setReport(report2, /*isDemo=*/false);

            // Очікуємо сигнал про успішне збереження файлу
            QVERIFY(completedSpy.wait(5000));
            QCOMPARE(completedSpy.count(), 1);

            // Файл повинен бути успішно збережений, а статусний текст містити підтвердження
            QVERIFY(QFile::exists(exportFile));
            QVERIFY(statusLabel->text().contains(QString::fromUtf8("Успішно збережено")));
            QVERIFY(statusLabel->text().contains(exportFile));
            QVERIFY(!exportPage.isExporting());
        }
    }

    // 5. Інтеграційний тест MainWindow: зв'язок Coordinator -> ComparisonPage, TrendPage, ExportPage
    void testMainWindowReviewIntegration() {
        xray::app::MainWindow window(xray::ui::Theme::dark);
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(1180, 760);
        window.show();
        qApp->processEvents();

        auto* nav = window.findChild<xray::ui::NavigationPanel*>();
        QVERIFY(nav != nullptr);

        // Перехід до розділу «Огляд» (Review), сторінка 0 (Порівняння)
        nav->setCurrentPage(xray::ui::Section::review, 0);
        qApp->processEvents();

        auto* compTable = window.findChild<QTableWidget*>("comparisonReviewTable");
        QVERIFY(compTable != nullptr);
        QVERIFY(compTable->isVisible());

        // Перевіряємо реакцію на comparisonCompleted: координатор передає звіт
        auto realRep = makeRealComparisonReport();
        emit window.coordinator()->comparisonCompleted(realRep);
        qApp->processEvents();

        // Передаємо історію репозиторію з точним комітом і шляхом для перевірки Code Churn
        xray::history::HistoryResult hist;
        xray::history::CommitRecord tipCommit;
        tipCommit.oid = "git_oid_target_222";
        hist.commits.push_back(tipCommit);

        // [P2] Файл змінився в проміжному коміті, а цільовий коміт змінював інший файл
        xray::history::ChangeRecord mathChange;
        mathChange.commitOid = "intermediate_commit_150";
        mathChange.newPath = "src/math.cpp";
        mathChange.oldPath = "src/math.cpp";
        mathChange.status = xray::history::ChangeStatus::modified;
        mathChange.addedLines = 5;
        mathChange.deletedLines = 2;
        hist.changes.push_back(mathChange);

        xray::history::ChangeRecord dummyChange;
        dummyChange.commitOid = "git_oid_target_222";
        dummyChange.newPath = "src/other.cpp";
        dummyChange.status = xray::history::ChangeStatus::added;
        dummyChange.addedLines = 10;
        dummyChange.deletedLines = 0;
        hist.changes.push_back(dummyChange);

        emit window.coordinator()->repositoryQueried(hist);
        qApp->processEvents();

        auto* compPage = window.findChild<xray::ui::review::ComparisonPage*>();
        QVERIFY(compPage != nullptr);
        // [P1] Перевіряємо, що беруться Git commitOid зі знімків, а не analysis ID!
        QCOMPARE(compPage->baseOid(), QString("git_oid_base_111"));
        QCOMPARE(compPage->targetOid(), QString("git_oid_target_222"));

        auto* trPage = window.findChild<xray::ui::review::TrendPage*>();
        QVERIFY(trPage != nullptr);
        // [P1] Перевіряємо tipOid для тренду — це commitOid цільового коміту!
        QCOMPARE(trPage->repositoryTip(), QString("git_oid_target_222"));

        // [P1] Перевіряємо, що в списку сутностей тренду міститься саме identityKey ("add(int,int)"), а не тільки коротке ім'я "add"
        auto* entityCombo = window.findChild<QComboBox*>("trendEntityCombo");
        QVERIFY(entityCombo != nullptr);
        bool foundIdentityKey = false;
        for (int i = 0; i < entityCombo->count(); ++i) {
            if (entityCombo->itemData(i).toString().contains("add(int,int)") ||
                entityCombo->itemData(i).toString().contains("multiply(int,int)")) {
                foundIdentityKey = true;
                break;
            }
        }
        QVERIFY(foundIdentityKey);

        // Перехід до сторінки 1 (Динаміка)
        nav->setCurrentPage(xray::ui::Section::review, 1);
        qApp->processEvents();

        auto* trendPoints = window.findChild<QTableWidget*>("trendPointsTable");
        QVERIFY(trendPoints != nullptr);
        QVERIFY(trendPoints->isVisible());

        // Перехід до сторінки 3 (Експорт)
        nav->setCurrentPage(xray::ui::Section::review, 3);
        qApp->processEvents();

        auto* exportPreview = window.findChild<QTextEdit*>("exportPreviewArea");
        QVERIFY(exportPreview != nullptr);
        QVERIFY(exportPreview->isVisible());

        // Перевірка відкриття картки: звичайне виділення рядка (наприклад, клавіатурою) НЕ повинно відкривати картку
        nav->setCurrentPage(xray::ui::Section::review, 0);
        qApp->processEvents();
        auto* changeCard = window.findChild<xray::ui::review::ChangeCardPage*>();
        QVERIFY(changeCard != nullptr);
        QVERIFY(compTable->isVisible());
        QVERIFY(!changeCard->isVisible());

        compTable->selectRow(0);
        qApp->processEvents();
        // Сторінка все ще має залишатися «Порівняння», клавіатурна навігація не перемикає на картку
        QVERIFY(compTable->isVisible());
        QVERIFY(!changeCard->isVisible());

        // Відкриття картки відбувається лише за явною дією користувача (клік по комірці)
        emit compTable->cellClicked(0, 0);
        qApp->processEvents();
        QVERIFY(changeCard->isVisible()); // Картка зміни (ChangeCardPage) відкрита!

        auto* histBtn = window.findChild<QPushButton*>("changeCardHistoryBtn");
        QVERIFY(histBtn != nullptr);
        QVERIFY(histBtn->isEnabled());

        // Натискаємо дію «Історія файла»
        histBtn->click();
        qApp->processEvents();

        // Перевіряємо перехід до розділу «Історія» на сторінку Code Churn
        auto* churnPage = window.findChild<xray::ui::history::CodeChurnPage*>();
        QVERIFY(churnPage != nullptr);
        QVERIFY(churnPage->isVisible());
        QVERIFY(!changeCard->isVisible());

        auto* churnPath = window.findChild<QLineEdit*>("churnPathEdit");
        QVERIFY(churnPath != nullptr);
        QCOMPARE(churnPath->text(), QString("src/math.cpp"));

        // Перевіряємо рядки результату: зміна з проміжного коміту відображається, історія не порожня
        auto* churnTable = window.findChild<QTableWidget*>("churnChangesTable");
        QVERIFY(churnTable != nullptr);
        QCOMPARE(churnTable->rowCount(), 1);
        QCOMPARE(churnTable->item(0, 1)->text(), QString("src/math.cpp"));
        QCOMPARE(churnTable->item(0, 5)->text(), QString("intermediate_commit_150"));

        // Перевірка 1: після «Скинути» відновлюється повна загальна історія репозиторію (всі файли)
        auto* resetBtn = window.findChild<QPushButton*>("churnResetBtn");
        QVERIFY(resetBtn != nullptr);
        resetBtn->click();
        qApp->processEvents();

        // У загальній історії репозиторію було 2 файли (src/math.cpp та src/other.cpp)
        QCOMPARE(churnTable->rowCount(), 2);
        QVERIFY(churnPath->text().isEmpty());

        // Перевірка 2: запізнілий результат запиту після «Скинути» відкидається і не перезаписує вибір користувача
        xray::history::HistoryResult lateHist;
        lateHist.changes = { mathChange };
        emit window.coordinator()->fileHistoryQueried(QString("src/math.cpp"), QString("intermediate_commit_150"), lateHist);
        qApp->processEvents();

        // Результат відкинуто: фільтр не увімкнувся назад, у таблиці залишаються всі 2 файли, а поле шляху порожнє
        QCOMPARE(churnTable->rowCount(), 2);
        QVERIFY(churnPath->text().isEmpty());

        // Перевірка 3: зміна фільтра користувачем (ввід у поле пошуку) також призводить до відкидання пізнього результату
        churnPath->setText("src/other");
        emit window.coordinator()->fileHistoryQueried(QString("src/math.cpp"), QString("intermediate_commit_150"), lateHist);
        qApp->processEvents();
        QCOMPARE(churnPath->text(), QString("src/other")); // вибір користувача не перезаписано

        // Скидаємо фільтр знову
        resetBtn->click();
        qApp->processEvents();
        QCOMPARE(churnTable->rowCount(), 2);

        // Перевірка 4: якщо запит повернув порожню історію, сторінка показує порожній список (0 рядків), а не підставляє загальний список репозиторію
        churnPage->filterByFileAndCommit(QString("src/math.cpp"), QString("intermediate_commit_150"));
        xray::history::HistoryResult emptyHist;
        emptyHist.changes = {};
        emit window.coordinator()->fileHistoryQueried(QString("src/math.cpp"), QString("intermediate_commit_150"), emptyHist);
        qApp->processEvents();
        QCOMPARE(churnTable->rowCount(), 0);

        // Перевірка 5: відображення обмеження історії (hasMore), бейдж показує часткові дані
        churnPage->filterByFileAndCommit(QString("src/math.cpp"), QString("intermediate_commit_150"));
        xray::history::HistoryResult partialHist;
        partialHist.hasMore = true;
        partialHist.changes = { mathChange };
        emit window.coordinator()->fileHistoryQueried(QString("src/math.cpp"), QString("intermediate_commit_150"), partialHist);
        qApp->processEvents();

        auto* completenessBadge = window.findChild<QLabel*>("churnCompletenessBadge");
        QVERIFY(completenessBadge != nullptr);
        QVERIFY(completenessBadge->text().contains(QString::fromUtf8("Часткові дані")));
        QCOMPARE(churnTable->rowCount(), 1);

        // Після «Скинути» знову відновлюється повна історія всіх файлів із позначкою «Повний підрахунок»
        resetBtn->click();
        qApp->processEvents();
        QCOMPARE(churnTable->rowCount(), 2);
        QCOMPARE(completenessBadge->text(), QString::fromUtf8("Повний підрахунок"));

        window.close();
    }

    // 6. [P2] Перевірка вибору OID для unmatched: OID вибирається за sourceId фактичного посилання
    void testChangeCard_UnmatchedOidFromActualSourceRef() {
        xray::ui::review::ChangeCardPage page;
        auto rep = makeRealComparisonReport();

        // 1. Unmatched сутність, присутня лише в базовому знімку
        xray::review::ReviewItem itemBase;
        itemBase.kind = xray::review::ReviewKind::unmatched;
        itemBase.entityKey = "legacyFunction()";
        xray::review::SourceRef baseRef;
        baseRef.sourceId = rep.base->parsed->source->id;
        baseRef.relativePath = "src/legacy.cpp";
        itemBase.sourceRefs = { baseRef };

        page.showItem(itemBase, rep);

        QSignalSpy spy(&page, &xray::ui::review::ChangeCardPage::fileHistoryRequested);
        auto* histBtn = findControl<QPushButton>(&page, "changeCardHistoryBtn");
        QVERIFY(histBtn->isEnabled());
        histBtn->click();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("src/legacy.cpp"));
        // Перевіряємо, що OID взято з БАЗОВОГО знімка (git_oid_base_111), а не цільового!
        QCOMPARE(spy.at(0).at(1).toString(), QString("git_oid_base_111"));

        // 2. Unmatched сутність, присутня лише в цільовому знімку
        xray::review::ReviewItem itemTarget;
        itemTarget.kind = xray::review::ReviewKind::unmatched;
        itemTarget.entityKey = "futureFunction()";
        xray::review::SourceRef targetRef;
        targetRef.sourceId = rep.target->parsed->source->id;
        targetRef.relativePath = "src/future.cpp";
        itemTarget.sourceRefs = { targetRef };

        page.showItem(itemTarget, rep);
        histBtn->click();

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toString(), QString("src/future.cpp"));
        // Перевіряємо, що OID взято з ЦІЛЬОВОГО знімка (git_oid_target_222)!
        QCOMPARE(spy.at(1).at(1).toString(), QString("git_oid_target_222"));
    }
};

QTEST_MAIN(ReviewUiTests)
#include "ui_tests.moc"
