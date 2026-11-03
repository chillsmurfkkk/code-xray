#pragma once

#include <QWidget>

#include "code/types.hpp"

#include <memory>

class QLabel;
class QTableWidget;
class QLineEdit;
class QComboBox;
class QPushButton;

namespace xray::ui {

class DiagnosticsPage : public QWidget {
    Q_OBJECT

public:
    explicit DiagnosticsPage(QWidget* parent = nullptr);
    void setSnapshot(
        std::shared_ptr<const code::ParsedSnapshot> snapshot,
        bool demonstration = false
    );
    void setLoading(bool loading);

signals:
    void reparseRequested();
    void openCodeRequested(const QString& path, int line);

private:
    void applyFilters();
    void updateActions();
    void openSelected();
    QLabel* status_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLineEdit* pathFilter_ = nullptr;
    QComboBox* kindFilter_ = nullptr;
    QPushButton* reparse_ = nullptr;
    QPushButton* open_ = nullptr;
    std::shared_ptr<const code::ParsedSnapshot> snapshot_;
    bool demonstration_ = false;
    bool loading_ = false;
};

} // namespace xray::ui
