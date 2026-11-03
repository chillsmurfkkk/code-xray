#pragma once

#include "code/types.hpp"

#include <QWidget>

#include <cstddef>
#include <memory>
#include <optional>

class QLabel;
class QLineEdit;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QComboBox;

namespace xray::ui {

class CodeView;

class StructurePage : public QWidget {
    Q_OBJECT

public:
    explicit StructurePage(QWidget* parent = nullptr);

    // Call on the GUI thread. Passing nullptr restores the no-source state.
    void setSnapshot(
        std::shared_ptr<const xray::code::ParsedSnapshot> snapshot,
        bool demonstration = false
    );

    void setLoading(bool loading);
    bool openLocation(const QString& path, int line);

    signals:
        void openDirectoryRequested();
        void cancelRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    std::size_t appendEntity(
        QTreeWidgetItem* parent,
        const xray::code::CodeEntity& entity,
        std::size_t unitIndex
    );

    std::shared_ptr<const xray::code::ParsedSnapshot> snapshot_;
    std::optional<std::size_t> displayedUnit_;

    QLabel* context_ = nullptr;
    QLabel* count_ = nullptr;
    QLineEdit* sourcePath_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    CodeView* codeView_ = nullptr;
    QStackedWidget* treeContent_ = nullptr;
    QStackedWidget* codeContent_ = nullptr;
    QWidget* emptyResult_ = nullptr;
    QWidget* noMatches_ = nullptr;
    QWidget* unavailableSource_ = nullptr;
    QPushButton* openButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QLineEdit* searchInput_ = nullptr;
    QComboBox* kindFilter_ = nullptr;

    void applyFilters();

    bool filterItem(
        QTreeWidgetItem* item,
        const QString& query,
        std::optional<xray::code::EntityKind> kind,
        std::size_t& matchCount
    );

    enum ItemRole {
        UnitIndexRole = Qt::UserRole,
        StartLineRole,
        EndLineRole,
        KindRole
    };

    void onEntitySelected(QTreeWidgetItem* item);
    void showEntityRange(QTreeWidgetItem* item);
    void updateRangeHighlight();
};

} // namespace xray::ui
