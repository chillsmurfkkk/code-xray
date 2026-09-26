//#include <iostream>
//#include <variant>
//#include "common/job.hpp"
//#include "common/result.hpp"
//#include "history/api.hpp"
//
//int main() {
//    using namespace xray;
//    using namespace xray::history;
//
//    RepositorySpec repo{ "." };
//    HistoryRequest req;
//    req.startOid = "HEAD";
//    req.relativePath = "src/app/main.cpp";
//    JobContext ctx;
//
//    auto res = query(repo, req, ctx);
//
//    if (std::holds_alternative<HistoryResult>(res)) {
//        std::cout << "Query successful!\n";
//        const auto& data = std::get<HistoryResult>(res);
//
//        if (!data.commits.empty()) {
//            std::cout << "Commit OID: " << data.commits[0].oid << "\n";
//            std::cout << "Author: " << data.commits[0].authorName << "\n";
//            std::cout << "Message: " << data.commits[0].message << "\n";
//        }
//
//        if (!data.changes.empty()) {
//            std::cout << "Changed File: " << data.changes[0].newPath.value_or("N/A") << "\n";
//            std::cout << "Lines Added: " << data.changes[0].addedLines.value_or(0) << "\n";
//        }
//
//        AuthorFilter filter("C");
//        if (!data.commits.empty()) {
//            bool matches = filter.matches(data.commits[0]);
//            std::cout << "AuthorFilter match status: " << (matches ? "True" : "False") << "\n";
//        }
//    }
//    else if (std::holds_alternative<Error>(res)) {
//        const auto& err = std::get<Error>(res);
//        std::cout << "[FAIL] Query error: " << err.message << "\n";
//    }
//
//    std::cout << "\nPress Enter to exit...";
//    std::cin.get();
//    return 0;
//}

#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QMainWindow window;
    window.setWindowTitle("Code Xray — E0");
    auto* message = new QLabel(QString::fromUtf8("Каркас Code Xray готовий. Модулі A–D ще в розробці."));
    message->setAlignment(Qt::AlignCenter);
    window.setCentralWidget(message);
    window.resize(960, 640);
    window.show();
    if (application.arguments().contains("--smoke-test")) {
        QTimer::singleShot(100, &application, &QApplication::quit);
    }
    return application.exec();
}

