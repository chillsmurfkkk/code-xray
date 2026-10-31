#pragma once

#include "code/types.hpp"
#include "common/job.hpp"

#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;

namespace xray::ui {

class SourcePage : public QWidget {
    Q_OBJECT

public:
    explicit SourcePage(QWidget* parent = nullptr);

    QString directory() const;
    code::SourceRequest sourceRequest() const;
    void setDirectory(const QString& directory);
    void addFiles(const QStringList& paths);
    void setLoading(bool loading);
    void setProgress(const Progress& progress);
    void setCancelling();
    void showResult(const QString& message, bool canOpenStructure = false);

signals:
    void browseDirectoryRequested();
    void browseFilesRequested();
    void scanRequested();
    void cancelRequested();
    void structureRequested();

private:
    void updateActions();
    void sourceChanged();
    void requestScan();

    QLineEdit* directoryInput_ = nullptr;
    QPushButton* browseButton_ = nullptr;
    QPushButton* filesButton_ = nullptr;
    QListWidget* filesList_ = nullptr;
    QLabel* selectionLabel_ = nullptr;
    QPushButton* removeButton_ = nullptr;
    QPushButton* clearButton_ = nullptr;
    QPushButton* scanButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QPushButton* structureButton_ = nullptr;
    QLabel* status_ = nullptr;
    QProgressBar* progress_ = nullptr;

    bool loading_ = false;
    bool cancelling_ = false;
};

} // namespace xray::ui
