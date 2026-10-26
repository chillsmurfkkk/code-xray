#include "structure_page.hpp"
#include "ui/shared/icons.hpp"

#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace xray::ui {
namespace {

class SplitterGrip final : public QSplitterHandle {
public:
    SplitterGrip(Qt::Orientation orientation, QSplitter* parent)
        : QSplitterHandle(orientation, parent) { setAttribute(Qt::WA_Hover); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QColor color = palette().color(underMouse() ? QPalette::Accent : QPalette::Mid);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        const QRectF grip(width() / 2.0 - 1.5, height() / 2.0 - 16, 3, 32);
        painter.drawRoundedRect(grip, 1.5, 1.5);
    }
    void enterEvent(QEnterEvent* event) override { QSplitterHandle::enterEvent(event); update(); }
    void leaveEvent(QEvent* event) override { QSplitterHandle::leaveEvent(event); update(); }
};

class WorkspaceSplitter final : public QSplitter {
public:
    explicit WorkspaceSplitter(QWidget* parent) : QSplitter(Qt::Horizontal, parent) {}
protected:
    QSplitterHandle* createHandle() override { return new SplitterGrip(orientation(), this); }
};

QWidget* emptyState(Symbol symbol, const QString& title, const QString& description, QWidget* parent) {
    auto* state = new QWidget(parent);
    auto* layout = new QVBoxLayout(state);
    layout->setContentsMargins(12, 24, 12, 24);
    layout->setSpacing(12);
    auto* mark = new SymbolView(symbol, state);
    auto* heading = new QLabel(title, state);
    heading->setObjectName("emptyTitle");
    heading->setWordWrap(true);
    heading->setAlignment(Qt::AlignCenter);
    auto* text = new QLabel(description, state);
    text->setObjectName("emptyDescription");
    text->setWordWrap(true);
    text->setAlignment(Qt::AlignCenter);
    layout->addStretch();
    layout->addWidget(mark, 0, Qt::AlignHCenter);
    layout->addWidget(heading);
    // Let the layout use the same width for measuring and laying out wrapped text.
    layout->addWidget(text);
    layout->addStretch();
    return state;
}

} // namespace

StructurePage::StructurePage(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(20);
    auto* title = new QLabel(QString::fromUtf8("Структура"), this);
    auto headingFont = title->font();
    headingFont.setPointSizeF(headingFont.pointSizeF() + 7);
    headingFont.setWeight(QFont::DemiBold);
    title->setFont(headingFont);
    context_ = new QLabel(QString::fromUtf8("Джерело ще не відкрито"), this);
    context_->setObjectName("sourceContext");
    context_->setWordWrap(true);
    auto* heading = new QVBoxLayout;
    heading->setSpacing(4);
    heading->addWidget(title);
    heading->addWidget(context_);
    layout->addLayout(heading);

    auto* splitter = new WorkspaceSplitter(this);
    splitter->setObjectName("structureSplit");
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(12);

    auto* treePanel = new QFrame(splitter);
    treePanel->setObjectName("workPanel");
    treePanel->setMinimumWidth(190);
    auto* treeLayout = new QVBoxLayout(treePanel);
    treeLayout->setContentsMargins(16, 18, 16, 16);
    treeLayout->setSpacing(10);
    auto* treeHeader = new QHBoxLayout;
    auto* treeTitle = new QLabel(QString::fromUtf8("Сутності"), treePanel);
    treeTitle->setObjectName("panelTitle");
    auto* count = new QLabel(QString::fromUtf8("—"), treePanel);
    count->setObjectName("countPill");
    count->setToolTip(QString::fromUtf8("Джерело ще не відкрито"));
    treeHeader->addWidget(treeTitle);
    treeHeader->addStretch();
    treeHeader->addWidget(count);
    treeLayout->addLayout(treeHeader);

    auto* treeContent = new QStackedWidget(treePanel);
    treeContent->setObjectName("structureContent");
    tree_ = new QTreeWidget(treeContent);
    tree_->setObjectName("structureTree");
    tree_->setHeaderHidden(true);
    tree_->setColumnCount(1);
    tree_->setAccessibleName(QString::fromUtf8("Структура коду"));
    tree_->setEnabled(false);
    treeContent->addWidget(emptyState(Symbol::structure,
        QString::fromUtf8("Поки немає структури"),
        QString::fromUtf8("Після відкриття джерела тут з’являться файли, типи та функції."), treeContent));
    treeContent->addWidget(tree_);
    treeContent->setCurrentIndex(0);
    treeLayout->addWidget(treeContent, 1);

    auto* codePanel = new QFrame(splitter);
    codePanel->setObjectName("workPanel");
    codePanel->setMinimumWidth(260);
    auto* codeLayout = new QVBoxLayout(codePanel);
    codeLayout->setContentsMargins(16, 18, 16, 16);
    codeLayout->setSpacing(10);
    auto* codeHeader = new QHBoxLayout;
    auto* codeTitle = new QLabel(QString::fromUtf8("Код файла"), codePanel);
    codeTitle->setObjectName("panelTitle");
    auto* readOnly = new QLabel(QString::fromUtf8("Лише перегляд"), codePanel);
    readOnly->setObjectName("statusPill");
    codeHeader->addWidget(codeTitle);
    codeHeader->addStretch();
    codeHeader->addWidget(readOnly);
    codeLayout->addLayout(codeHeader);

    auto* codeContent = new QStackedWidget(codePanel);
    codeContent->setObjectName("sourceContent");
    codeView_ = new QPlainTextEdit(codeContent);
    codeView_->setObjectName("sourceCode");
    codeView_->setReadOnly(true);
    codeView_->setLineWrapMode(QPlainTextEdit::NoWrap);
    codeView_->setTabChangesFocus(true);
    codeView_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    codeView_->setAccessibleName(QString::fromUtf8("Код файла"));
    codeContent->addWidget(emptyState(Symbol::document,
        QString::fromUtf8("Виберіть файл"),
        QString::fromUtf8("Його код буде показано тут. Текст можна буде виділити та скопіювати."), codeContent));
    codeContent->addWidget(codeView_);
    codeContent->setCurrentIndex(0);
    codeLayout->addWidget(codeContent, 1);

    splitter->addWidget(treePanel);
    splitter->addWidget(codePanel);
    splitter->setSizes({300, 700});
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 7);
    layout->addWidget(splitter, 1);
}

} // namespace xray::ui
