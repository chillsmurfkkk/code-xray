#pragma once

#include <QIcon>
#include <QToolButton>
#include <QWidget>

namespace xray::ui {

enum class Symbol {
    code, analysis, history, compare, source, filter, structure, diagnostics,
    sidebarClose, sidebarOpen, appearance, document
};

// Vector strokes use the current application palette at every requested scale.
QIcon makeIcon(Symbol symbol);

class IconButton : public QToolButton {
public:
    IconButton(Symbol symbol, const QString& title, QWidget* parent = nullptr);
    void setDescription(const QString& description);
protected:
    bool event(QEvent* event) override;
};

class SymbolView : public QWidget {
public:
    explicit SymbolView(Symbol symbol, QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    Symbol symbol_;
};

} // namespace xray::ui
