#include "structure_page.hpp"
#include "ui/shared/icons.hpp"

#include <QApplication>
#include <QEvent>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <QTreeWidgetItem>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFormat>
#include <QVariant>
#include <QPushButton>
#include <QComboBox>

#include <algorithm>
#include <limits>
#include <utility>

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

class EntityDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
    ) const override {
        QStyleOptionViewItem adjusted = option;
        initStyleOption(&adjusted, index);

        if (adjusted.state & QStyle::State_Selected) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(
                adjusted.palette.brush(QPalette::Highlight)
            );

            painter->drawRoundedRect(
                QRectF(adjusted.rect).adjusted(0, 1, 0, -1),
                6,
                6
            );

            painter->restore();
        }

        adjusted.state &= ~(
            QStyle::State_Selected | QStyle::State_HasFocus
        );

        QStyledItemDelegate::paint(painter, adjusted, index);
    }
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
    auto* openButton = new QPushButton(
        QString::fromUtf8("Відкрити папку"),
        this
    );
    openButton->setObjectName("openSourceButton");
    openButton->setProperty("role", "primaryAction");
    openButton->setIcon(makeIcon(Symbol::source));
    openButton->setIconSize(QSize(18, 18));
    openButton->setMinimumHeight(36);
    openButton->setCursor(Qt::PointingHandCursor);

    openButton_ = openButton;

    cancelButton_ = new QPushButton(
        QString::fromUtf8("Скасувати"),
        this
    );
    cancelButton_->setObjectName("cancelSourceButton");
    cancelButton_->setProperty("role", "primaryAction");
    cancelButton_->setMinimumHeight(36);
    cancelButton_->setCursor(Qt::PointingHandCursor);
    cancelButton_->hide();

    connect(
        cancelButton_,
        &QPushButton::clicked,
        this,
        [this]() {
            cancelButton_->setEnabled(false);
            cancelButton_->setText(
                QString::fromUtf8("Скасовуємо…")
            );
            emit cancelRequested();
        }
    );

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(16);
    headerRow->addLayout(heading, 1);
    headerRow->addWidget(openButton, 0, Qt::AlignVCenter);
    headerRow->addWidget(cancelButton_, 0, Qt::AlignVCenter);

    layout->addLayout(headerRow);

    connect(
        openButton,
        &QPushButton::clicked,
        this,
        &StructurePage::openDirectoryRequested
    );

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

    auto* searchInput = new QLineEdit(treePanel);
    searchInput->setObjectName("entitySearch");
    searchInput->setProperty("role", "inputField");
    searchInput->setMinimumHeight(36);
    searchInput->setMinimumWidth(0);
    searchInput->setPlaceholderText(
        QString::fromUtf8("Пошук за назвою")
    );
    searchInput->setAccessibleName(
        QString::fromUtf8("Пошук сутностей")
    );
    searchInput->setClearButtonEnabled(true);

    treeLayout->addWidget(searchInput);

    using xray::code::EntityKind;

    auto* kindFilter = new QComboBox(treePanel);
    kindFilter->setObjectName("entityKindFilter");
    kindFilter->setProperty("role", "inputField");
    kindFilter->setAccessibleName(
        QString::fromUtf8("Вид сутності")
    );
    kindFilter->setMinimumHeight(36);
    kindFilter->setMinimumWidth(0);

    kindFilter->addItem(QString::fromUtf8("Усі сутності"));
    kindFilter->addItem(
        QString::fromUtf8("Файли"),
        static_cast<int>(EntityKind::file)
    );
    kindFilter->addItem(
        QString::fromUtf8("Типи"),
        static_cast<int>(EntityKind::type)
    );
    kindFilter->addItem(
        QString::fromUtf8("Функції"),
        static_cast<int>(EntityKind::function)
    );

    treeLayout->addWidget(kindFilter);

    searchInput_ = searchInput;
    kindFilter_ = kindFilter;

    connect(
        searchInput_,
        &QLineEdit::textChanged,
        this,
        &StructurePage::applyFilters
    );

    connect(
        kindFilter_,
        &QComboBox::currentIndexChanged,
        this,
        &StructurePage::applyFilters
    );

    auto* treeContent = new QStackedWidget(treePanel);
    treeContent->setObjectName("structureContent");
    tree_ = new QTreeWidget(treeContent);
    tree_->setItemDelegate(new EntityDelegate(tree_));
    tree_->setObjectName("structureTree");
    tree_->setHeaderHidden(true);
    tree_->setColumnCount(1);
    tree_->setUniformRowHeights(true);
    tree_->setWordWrap(false);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->setAccessibleName(QString::fromUtf8("Структура коду"));
    tree_->setEnabled(false);
    treeContent->addWidget(emptyState(Symbol::structure,
        QString::fromUtf8("Поки немає структури"),
        QString::fromUtf8("Після відкриття джерела тут з’являться файли, типи та функції."), treeContent));
    treeContent->addWidget(tree_);
    emptyResult_ = emptyState(Symbol::structure,
        QString::fromUtf8("Сутностей не знайдено"),
        QString::fromUtf8("У цьому знімку немає доступних файлів, типів або функцій."), treeContent);
    treeContent->addWidget(emptyResult_);

    noMatches_ = emptyState(
        Symbol::filter,
        QString::fromUtf8("Збігів немає"),
        QString::fromUtf8(
            "Змініть пошук або скиньте фільтри."
        ),
        treeContent
    );
    noMatches_->setObjectName("noFilterMatches");

    auto* resetButton = new QPushButton(
        QString::fromUtf8("Скинути фільтри"),
        noMatches_
    );
    resetButton->setObjectName("resetEntityFilters");
    resetButton->setProperty("role", "primaryAction");
    resetButton->setMinimumHeight(36);

    auto* stateLayout =
        static_cast<QVBoxLayout*>(noMatches_->layout());

    stateLayout->insertWidget(
        stateLayout->count() - 1,
        resetButton,
        0,
        Qt::AlignHCenter
    );

    treeContent->addWidget(noMatches_);

    connect(resetButton, &QPushButton::clicked, this, [this]() {
        const QSignalBlocker searchBlocker(searchInput_);
        const QSignalBlocker kindBlocker(kindFilter_);

        searchInput_->clear();
        kindFilter_->setCurrentIndex(0);

        applyFilters();
        searchInput_->setFocus();
    });

    treeContent->setCurrentIndex(0);
    treeLayout->addWidget(treeContent, 1);

    auto* codePanel = new QFrame(splitter);
    codePanel->setObjectName("workPanel");
    codePanel->setMinimumWidth(260);
    auto* codeLayout = new QVBoxLayout(codePanel);
    codeLayout->setContentsMargins(16, 18, 16, 16);
    codeLayout->setSpacing(10);
    auto* codeHeader = new QHBoxLayout;
    sourcePath_ = new QLineEdit(codePanel);
    sourcePath_->setObjectName("sourcePath");
    sourcePath_->setReadOnly(true);
    sourcePath_->setPlaceholderText(QString::fromUtf8("Код файла"));
    sourcePath_->setAccessibleName(QString::fromUtf8("Шлях вибраного файла"));
    sourcePath_->setMinimumWidth(0);
    auto* readOnly = new QLabel(QString::fromUtf8("Лише перегляд"), codePanel);
    readOnly->setObjectName("statusPill");
    codeHeader->addWidget(sourcePath_, 1);
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
    unavailableSource_ = emptyState(Symbol::document,
        QString::fromUtf8("Код недоступний"),
        QString::fromUtf8("У знімку немає тексту вибраного файла."), codeContent);
    codeContent->addWidget(unavailableSource_);
    codeContent->setCurrentIndex(0);
    codeLayout->addWidget(codeContent, 1);

    splitter->addWidget(treePanel);
    splitter->addWidget(codePanel);
    splitter->setSizes({300, 700});
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 7);
    layout->addWidget(splitter, 1);

    count_ = count;
    treeContent_ = treeContent;
    codeContent_ = codeContent;

    connect(
        tree_,
        &QTreeWidget::currentItemChanged,
        this,
        &StructurePage::onEntitySelected
    );
}

std::size_t StructurePage::appendEntity(
    QTreeWidgetItem* parent,
    const xray::code::CodeEntity& entity,
    std::size_t unitIndex
) {
    auto* item = parent
        ? new QTreeWidgetItem(parent)
        : new QTreeWidgetItem(tree_);

    item->setData(
        0,
        UnitIndexRole,
        static_cast<qulonglong>(unitIndex)
    );

    item->setData(
        0,
        StartLineRole,
        entity.kind == xray::code::EntityKind::file
            ? 0u
            : entity.range.startLine
    );

    item->setData(0, EndLineRole,
        entity.kind == xray::code::EntityKind::file ? 0u : entity.range.endLine
    );

    item->setData(
        0,
        KindRole,
        static_cast<int>(entity.kind)
    );

    const auto description = QString::fromStdString(entity.describe());
    item->setText(0, description);
    item->setToolTip(0, description);

    switch (entity.kind) {
    case xray::code::EntityKind::file:
        item->setIcon(0, makeIcon(Symbol::document));
        break;

    case xray::code::EntityKind::function:
        item->setIcon(0, makeIcon(Symbol::code));
        break;

    case xray::code::EntityKind::type:
        item->setIcon(0, makeIcon(Symbol::structure));
        break;
    }

    std::size_t count = 1;

    for (const auto& child : entity.children) {
        if (child) {
            count += appendEntity(item, *child, unitIndex);
        }
    }

    return count;
}

void StructurePage::setSnapshot(
    std::shared_ptr<const xray::code::ParsedSnapshot> snapshot,
    bool demonstration
) {
    const QSignalBlocker blocker(tree_);

    displayedUnit_.reset();

    tree_->clear();
    codeView_->clear();
    codeView_->setExtraSelections({});
    codeView_->setToolTip({});
    sourcePath_->clear();
    sourcePath_->setToolTip({});
    sourceLineStarts_.clear();

    snapshot_ = std::move(snapshot);

    tree_->setEnabled(false);
    treeContent_->setCurrentIndex(0);
    codeContent_->setCurrentIndex(0);

    count_->setText(QString::fromUtf8("—"));
    count_->setToolTip(QString::fromUtf8("Джерело ще не відкрито"));

    if (!snapshot_) {
        context_->setText(QString::fromUtf8("Джерело ще не відкрито"));
        return;
    }

    std::size_t entityCount = 0;

    for (std::size_t i = 0; i < snapshot_->units.size(); ++i) {
        const auto& unit = snapshot_->units[i];

        if (unit.root) {
            entityCount += appendEntity(nullptr, *unit.root, i);
        }
    }

    count_->setText(
        QString::number(static_cast<qulonglong>(entityCount))
    );
    count_->setToolTip(QString::fromUtf8("Кількість сутностей у дереві"));

    QString description = demonstration
        ? QString::fromUtf8("Демонстраційні дані · файлів: %1")
        : QString::fromUtf8("Файлів у результаті: %1");

    description = description.arg(
        static_cast<qulonglong>(snapshot_->units.size())
    );

    const bool partial =
        snapshot_->coverage.completeness == xray::Completeness::partial
        || (snapshot_->source
            && snapshot_->source->coverage.completeness
                == xray::Completeness::partial)
        || std::any_of(snapshot_->units.begin(), snapshot_->units.end(), [](const auto& unit) {
            return !unit.root || unit.coverage.completeness == xray::Completeness::partial;
        });

    if (partial) {
        description += QString::fromUtf8(" · частковий розбір");
    }

    if (entityCount == 0) {
        description += QString::fromUtf8(" · сутностей немає");
    }

    context_->setText(description);

    tree_->setEnabled(entityCount != 0);
    tree_->expandToDepth(1);
    treeContent_->setCurrentWidget(entityCount == 0 ? emptyResult_ : tree_);

    applyFilters();
}

void StructurePage::onEntitySelected(QTreeWidgetItem* item) {
    if (item && snapshot_ && displayedUnit_) {
        bool validIndex = false;
        const auto unitIndex = item->data(0, UnitIndexRole)
            .toULongLong(&validIndex);

        if (validIndex && unitIndex == *displayedUnit_) {
            showEntityRange(item);
            return;
        }
    }

    displayedUnit_.reset();

    codeView_->clear();
    codeView_->setExtraSelections({});
    codeView_->setToolTip({});
    sourcePath_->clear();
    sourcePath_->setToolTip({});
    sourceLineStarts_.clear();
    codeContent_->setCurrentIndex(0);

    if (!item || !snapshot_) {
        return;
    }

    bool validIndex = false;
    const auto unitIndex = item->data(0, UnitIndexRole)
        .toULongLong(&validIndex);

    if (!validIndex || unitIndex >= snapshot_->units.size()) {
        codeContent_->setCurrentWidget(unavailableSource_);
        return;
    }

    const auto& file =
        snapshot_->units[static_cast<std::size_t>(unitIndex)].root;

    if (!file || !file->sourceBytes) {
        codeContent_->setCurrentWidget(unavailableSource_);
        return;
    }

    const auto& bytes = *file->sourceBytes;

    QString text = QString::fromUtf8(bytes.data(), static_cast<qsizetype>(bytes.size()));
    if (text.startsWith(QChar::ByteOrderMark)) text.remove(0, 1);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    if (text.size() >= std::numeric_limits<int>::max()) {
        codeContent_->setCurrentWidget(unavailableSource_);
        return;
    }

    sourceLineStarts_.push_back(0);
    for (qsizetype position = 0; position < text.size(); ++position) {
        if (text.at(position) == QLatin1Char('\n'))
            sourceLineStarts_.push_back(static_cast<int>(position + 1));
    }
    codeView_->setPlainText(text);

    codeView_->setToolTip(
        QString::fromStdString(file->relativePath)
    );
    sourcePath_->setText(QString::fromStdString(file->relativePath));
    sourcePath_->setToolTip(sourcePath_->text());
    sourcePath_->setCursorPosition(0);

    codeContent_->setCurrentWidget(codeView_);
    displayedUnit_ = static_cast<std::size_t>(unitIndex);
    showEntityRange(item);
}

void StructurePage::showEntityRange(QTreeWidgetItem* item) {
    updateRangeHighlight();

    const auto startLine = item->data(0, StartLineRole)
        .toULongLong();

    if (startLine > sourceLineStarts_.size()) {
        return;
    }

    const int position = startLine == 0
        ? 0
        : sourceLineStarts_[
            static_cast<std::size_t>(startLine - 1)
        ];

    QTextCursor cursor(codeView_->document());
    cursor.setPosition(position);
    codeView_->setTextCursor(cursor);
    codeView_->ensureCursorVisible();
}

void StructurePage::updateRangeHighlight() {
    if (!tree_ || !codeView_ || !codeContent_) return;
    codeView_->setExtraSelections({});
    if (codeContent_->currentWidget() != codeView_) return;
    const auto* item = tree_->currentItem();
    if (!item) return;

    const auto start = item->data(0, StartLineRole).toULongLong();
    const auto end = item->data(0, EndLineRole).toULongLong();
    if (start == 0 || end < start || end > sourceLineStarts_.size()) return;

    auto* document = codeView_->document();
    const int lastPosition = end == sourceLineStarts_.size()
        ? document->characterCount() - 1
        : sourceLineStarts_[static_cast<std::size_t>(end)] - 1;
    const int firstPosition = sourceLineStarts_[static_cast<std::size_t>(start - 1)];
    if (lastPosition < firstPosition || lastPosition >= document->characterCount()) return;

    QTextEdit::ExtraSelection highlight;
    highlight.cursor = QTextCursor(document);
    highlight.cursor.setPosition(firstPosition);
    highlight.cursor.setPosition(lastPosition, QTextCursor::KeepAnchor);
    highlight.format.setBackground(QApplication::palette().brush(QPalette::Highlight));
    highlight.format.setProperty(QTextFormat::FullWidthSelection, true);
    // A multi-line selection stops at the final character. Add a full-width
    // overlay for its final visual line, including a closing brace at EOF.
    auto finalLine = highlight;
    finalLine.cursor.clearSelection();
    codeView_->setExtraSelections({highlight, finalLine});
}

void StructurePage::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        updateRangeHighlight();
}

void StructurePage::setLoading(bool loading) {
    openButton_->setEnabled(!loading);

    cancelButton_->setVisible(loading);
    cancelButton_->setEnabled(loading);
    cancelButton_->setText(
        QString::fromUtf8("Скасувати")
    );
}

bool StructurePage::filterItem(
    QTreeWidgetItem* item,
    const QString& query,
    std::optional<xray::code::EntityKind> kind,
    std::size_t& matchCount
) {
    bool hasVisibleChild = false;

    for (int i = 0; i < item->childCount(); ++i) {
        const bool childVisible = filterItem(
            item->child(i), query, kind, matchCount
        );

        hasVisibleChild = hasVisibleChild || childVisible;
    }

    const bool matchesText =
        item->text(0).contains(query, Qt::CaseInsensitive);

    const bool matchesKind =
        !kind
        || item->data(0, KindRole).toInt()
            == static_cast<int>(*kind);

    const bool matches = matchesText && matchesKind;

    if (matches) {
        ++matchCount;
    }

    const bool visible = matches || hasVisibleChild;

    item->setHidden(!visible);

    if ((!query.isEmpty() || kind.has_value())
        && hasVisibleChild) {
        item->setExpanded(true);
    }

    return visible;
}

void StructurePage::applyFilters() {
    if (!snapshot_) {
        return;
    }

    const QString query = searchInput_->text().trimmed();

    std::optional<xray::code::EntityKind> kind;
    const auto filterData = kindFilter_->currentData();

    if (filterData.isValid()) {
        kind = static_cast<xray::code::EntityKind>(
            filterData.toInt()
        );
    }

    std::size_t matchCount = 0;

    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        filterItem(
            tree_->topLevelItem(i),
            query,
            kind,
            matchCount
        );
    }

    auto* current = tree_->currentItem();

    if (current && current->isHidden()) {
        tree_->setCurrentItem(nullptr);
        tree_->clearSelection();
    }

    count_->setText(
        QString::number(
            static_cast<qulonglong>(matchCount)
        )
    );

    const bool filtering =
        !query.isEmpty() || kind.has_value();

    count_->setToolTip(
        filtering
            ? QString::fromUtf8(
                "Кількість збігів без батьківських контейнерів"
            )
            : QString::fromUtf8(
                "Кількість сутностей у дереві"
            )
    );

    if (tree_->topLevelItemCount() == 0) {
        treeContent_->setCurrentWidget(emptyResult_);
    } else if (matchCount == 0) {
        treeContent_->setCurrentWidget(noMatches_);
    } else {
        treeContent_->setCurrentWidget(tree_);
    }
}

} // namespace xray::ui
