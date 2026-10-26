#pragma once

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QTreeWidget;

namespace xray::ui {

class StructurePage : public QWidget {
public:
    explicit StructurePage(QWidget* parent = nullptr);

private:
    QLabel* context_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QPlainTextEdit* codeView_ = nullptr;
};

} // namespace xray::ui
