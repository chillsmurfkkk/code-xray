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
#include "ui/review/review_fixtures.hpp"
#include "app/main_window.hpp"
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
    rep.baseId = "commit_base_001";
    rep.targetId = "commit_target_002";
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
        page.setReport(realReport, /*isDemo=*/false, "commit_base_001", "commit_target_002");

        QVERIFY(demoBadge->isHidden());
        QVERIFY(!page.isDemo());
        QCOMPARE(page.currentReport().baseId, std::string("commit_base_001"));
        QCOMPARE(page.currentReport().targetId, std::string("commit_target_002"));

        // Перевіряємо сигнал запиту перебудови реального звіту
        QSignalSpy rebuildSpy(&page, &xray::ui::review::ComparisonPage::compareRebuildRequested);
        QSignalSpy compareSpy(&page, &xray::ui::review::ComparisonPage::compareRequested);

        auto* compareBtn = findControl<QPushButton>(&page, "comparisonCompareButton");
        compareBtn->click();

        // Реальний звіт НЕ замінено на демо!
        QVERIFY(!page.isDemo());
        QVERIFY(demoBadge->isHidden());
        QCOMPARE(page.currentReport().baseId, std::string("commit_base_001"));
        QCOMPARE(page.currentReport().targetId, std::string("commit_target_002"));

        // Сигнал compareRebuildRequested емітовано з правильними хешами комітів
        QCOMPARE(rebuildSpy.count(), 1);
        auto args = rebuildSpy.takeFirst();
        QCOMPARE(args.at(0).toString(), QString("commit_base_001"));
        QCOMPARE(args.at(1).toString(), QString("commit_target_002"));
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

        // Перевірка попереднього перегляду JSON
        previewBtn->click();
        QVERIFY(!previewArea->toPlainText().isEmpty());
        auto previewJson = nlohmann::json::parse(previewArea->toPlainText().toStdString(), nullptr, false);
        QVERIFY(!previewJson.is_discarded());

        // Збереження файлу
        saveBtn->click();
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
        QCOMPARE(parsed["baseId"].get<std::string>(), std::string("commit_base_001"));
        QCOMPARE(parsed["targetId"].get<std::string>(), std::string("commit_target_002"));

        // --- Експорт у HTML ---
        const int htmlIdx = formatCombo->findData(static_cast<int>(xray::review::ExportFormat::html));
        formatCombo->setCurrentIndex(htmlIdx >= 0 ? htmlIdx : 0);
        QString htmlPath = tempDir.filePath("test_report.html");
        pathEdit->setText(htmlPath);

        // Перевірка попереднього перегляду HTML
        previewBtn->click();
        QVERIFY(previewArea->toPlainText().contains("<html") || previewArea->toPlainText().contains("<!DOCTYPE"));

        // Збереження файлу
        saveBtn->click();
        QVERIFY(QFile::exists(htmlPath));
        QVERIFY(statusLabel->text().contains(QString::fromUtf8("Успішно збережено")));

        // Читання та верифікація згенерованого HTML
        QFile htmlFile(htmlPath);
        QVERIFY(htmlFile.open(QIODevice::ReadOnly | QIODevice::Text));
        QString htmlContent = QString::fromUtf8(htmlFile.readAll());
        htmlFile.close();

        QVERIFY(htmlContent.contains("<html", Qt::CaseInsensitive));
        QVERIFY(htmlContent.contains("commit_base_001"));
        QVERIFY(htmlContent.contains("commit_target_002"));
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

        window.close();
    }
};

QTEST_MAIN(ReviewUiTests)
#include "ui_tests.moc"
