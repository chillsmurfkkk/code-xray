#pragma once

#include "code/types.hpp"

#include <QWidget>

#include <cstddef>
#include <memory>
#include <vector>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace xray::ui {

class StructurePage : public QWidget {
    Q_OBJECT

public:
    explicit StructurePage(QWidget* parent = nullptr);

    // Call on the GUI thread. Passing nullptr restores the no-source state.
    void setSnapshot(
        std::shared_ptr<const xray::code::ParsedSnapshot> snapshot,
        bool demonstration = false
    );

    signals:
        void openDirectoryRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    std::size_t appendEntity(
        QTreeWidgetItem* parent,
        const xray::code::CodeEntity& entity,
        std::size_t unitIndex
    );

    std::shared_ptr<const xray::code::ParsedSnapshot> snapshot_;

    QLabel* context_ = nullptr;
    QLabel* count_ = nullptr;
    QLineEdit* sourcePath_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QPlainTextEdit* codeView_ = nullptr;
    QStackedWidget* treeContent_ = nullptr;
    QStackedWidget* codeContent_ = nullptr;
    QWidget* emptyResult_ = nullptr;
    QWidget* unavailableSource_ = nullptr;

    // Original LF-delimited source lines mapped to decoded document positions.
    // Qt also treats paragraph separators as blocks, so block numbers alone
    // cannot reliably represent source line numbers.
    std::vector<int> sourceLineStarts_;

    enum ItemRole {
        UnitIndexRole = Qt::UserRole,
        StartLineRole,
        EndLineRole
    };

    void onEntitySelected(QTreeWidgetItem* item);
    void updateRangeHighlight();
};

} // namespace xray::ui
