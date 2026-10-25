#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QTimer>

#include "ui/review/ComparisonPage.hpp"
#include "ui/review/TrendPage.hpp"
#include "ui/review/ChangeCardPage.hpp"
#include "ui/review/ExportPage.hpp"
#include "ui/review/review_fixtures.hpp"

// Фільтр повідомлень Qt для ігнорування внутрішніх асертів Win32-курсорів у Debug-режимі
static void customMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    if (msg.contains("Format_Mono") || msg.contains("qpixmap_win.cpp")) {
        return; // Ігноруємо відомий внутрішній баг Qt 6 на Windows 11
    }
    QByteArray localMsg = msg.toLocal8Bit();
    fprintf(stderr, "%s\n", localMsg.constData());
}

int main(int argc, char* argv[]) {
    qInstallMessageHandler(customMessageHandler);
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
    QApplication application(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QString::fromUtf8("Code Xray — Порівняння та звіти (Review)"));
    window.resize(1100, 720);

    auto* tabWidget = new QTabWidget(&window);

    auto* comparisonPage = new xray::ui::review::ComparisonPage(tabWidget);
    auto* trendPage      = new xray::ui::review::TrendPage(tabWidget);
    auto* changeCardPage = new xray::ui::review::ChangeCardPage(tabWidget);
    auto* exportPage     = new xray::ui::review::ExportPage(tabWidget);

    tabWidget->addTab(comparisonPage, QString::fromUtf8("Порівняння"));
    tabWidget->addTab(trendPage,      QString::fromUtf8("Динаміка"));
    tabWidget->addTab(changeCardPage, QString::fromUtf8("Картка зміни"));
    tabWidget->addTab(exportPage,     QString::fromUtf8("Експорт"));

    // Зв'язуємо вибір рядка в таблиці порівняння з відкриттям «Картки зміни»
    QObject::connect(comparisonPage, &xray::ui::review::ComparisonPage::reviewItemSelected,
        [tabWidget, changeCardPage](int index) {
            auto report = xray::ui::review::makeDemoComparisonReport();
            if (index >= 0 && index < static_cast<int>(report.items.size())) {
                changeCardPage->showItem(report.items[static_cast<std::size_t>(index)], report);
                tabWidget->setCurrentWidget(changeCardPage);
            }
        });

    window.setCentralWidget(tabWidget);
    window.show();

    if (application.arguments().contains("--smoke-test")) {
        QTimer::singleShot(100, &application, &QApplication::quit);
    }

    return application.exec();
}
