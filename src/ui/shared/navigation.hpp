#pragma once

#include "theme.hpp"
#include <QFrame>

class QLabel;
class QListWidget;

namespace xray::ui {
class IconButton;

enum class Section {
    code,
    analysis,
    history,
    review
};

class NavigationPanel : public QFrame {
    Q_OBJECT
public:
    explicit NavigationPanel(QWidget* parent = nullptr);
    void setTheme(Theme theme);

signals:
    void themeToggleRequested();
    void sectionChanged(Section section);
    void pageSelected(Section section, int pageIndex);

private:
    void setExpanded(bool expanded);
    void showSection(Section section);

    QWidget* rail_ = nullptr;
    QWidget* pagesPanel_ = nullptr;
    QFrame* divider_ = nullptr;
    IconButton* pagesToggle_ = nullptr;
    IconButton* themeButton_ = nullptr;
    IconButton* codeButton_ = nullptr;
    IconButton* compareButton_ = nullptr;

    QLabel* sectionTitle_ = nullptr;
    QLabel* sectionSubtitle_ = nullptr;
    QListWidget* pagesList_ = nullptr;

    Section currentSection_ = Section::code;
    int pagesWidth_ = 210;
};
} // namespace xray::ui
