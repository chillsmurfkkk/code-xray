#pragma once

#include "code/types.hpp"

#include <QString>
#include <QWidget>

class QLineEdit;
class QListWidget;

namespace xray::ui {

class FileSelectionPage : public QWidget {
    Q_OBJECT

public:
    explicit FileSelectionPage(QWidget* parent = nullptr);
    bool applyTo(code::FileSelection& selection, QString& error) const;

private:
    QLineEdit* extensionsInput_ = nullptr;
    QListWidget* excludedDirectories_ = nullptr;
};

} // namespace xray::ui
