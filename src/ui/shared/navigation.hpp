#pragma once

#include "theme.hpp"
#include <QFrame>

namespace xray::ui {
class IconButton;

class NavigationPanel : public QFrame {
    Q_OBJECT
public:
    explicit NavigationPanel(QWidget* parent = nullptr);
    void setTheme(Theme theme);
signals:
    void themeToggleRequested();
private:
    void setExpanded(bool expanded);
    QWidget* rail_ = nullptr;
    QWidget* pagesPanel_ = nullptr;
    QFrame* divider_ = nullptr;
    IconButton* pagesToggle_ = nullptr;
    IconButton* themeButton_ = nullptr;
    int pagesWidth_ = 210;
};
} // namespace xray::ui
