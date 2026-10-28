#include "ui/analysis/findings_page.hpp"
#include "ui/analysis/presentation.hpp"
#include "analysis/engine.hpp"
#include "ui/code/code_view.hpp"
#include <QApplication>
#include <QComboBox>
#include <QListView>
#include <QFontDatabase>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTextBlock>
#include <QTextEdit>
#include <QVBoxLayout>

namespace xray::ui::analysis_pages {
FindingsPage::FindingsPage(Session& session, QWidget* parent) : QWidget(parent), session_(session) {
    setObjectName("findingsPage");
    auto* layout = new QVBoxLayout(this);
    auto* filters = new QHBoxLayout;
    rule_ = new QComboBox;
    rule_->setProperty("role", "inputField");
    rule_->setView(new QListView);
    rule_->setObjectName("findingRule");
    rule_->addItem(tr("Усі правила"));
    for (int i = 0; i < 3; ++i)
        rule_->addItem(ruleName(i));
    search_ = new QLineEdit;
    search_->setProperty("role", "inputField");
    search_->setObjectName("findingSearch");
    search_->setClearButtonEnabled(true);
    search_->setPlaceholderText(tr("Пошук функції або пояснення…"));
    path_ = new QLineEdit;
    path_->setProperty("role", "inputField");
    path_->setObjectName("findingPath");
    path_->setClearButtonEnabled(true);
    path_->setPlaceholderText(tr("Фільтр шляху файла…"));
    filters->addWidget(rule_);
    filters->addWidget(search_, 1);
    filters->addWidget(path_, 1);
    layout->addLayout(filters);
    count_ = new QLabel;
    count_->setObjectName("findingCount");
    count_->setWordWrap(true);
    layout->addWidget(count_);
    auto* split = new QSplitter(Qt::Vertical);
    table_ = new QTableWidget;
    table_->setObjectName("findingsTable");
    prepareTable(table_, {tr("Правило"), tr("Функція"), tr("Файл:рядок"), tr("Значення"), tr("Поріг"),
                          tr("Точність")});
    split->addWidget(table_);
    auto* detail = new QWidget;
    auto* details = new QVBoxLayout(detail);
    explanation_ = new QLabel;
    explanation_->setObjectName("findingExplanation");
    explanation_->setWordWrap(true);
    explanation_->setTextFormat(Qt::PlainText);
    explanation_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    details->addWidget(explanation_);
    code_ = new xray::ui::CodeView;
    code_->setObjectName("findingCode");
    code_->setReadOnly(true);
    code_->installEventFilter(this);
    code_->setLineWrapMode(QPlainTextEdit::NoWrap);
    code_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    details->addWidget(code_);
    split->addWidget(detail);
    split->setSizes({280, 220});
    layout->addWidget(split);
    auto* buttons = new QHBoxLayout;
    navigate_ = new QPushButton(tr("Перейти до коду"));
    navigate_->setObjectName("navigateFinding");
    navigate_->setProperty("role", "primaryAction");
    reanalyze_ = new QPushButton(tr("Повторити аналіз"));
    reanalyze_->setObjectName("reanalyze");
    reanalyze_->setProperty("role", "primaryAction");
    buttons->addWidget(navigate_);
    buttons->addStretch();
    buttons->addWidget(reanalyze_);
    layout->addLayout(buttons);
    connect(rule_, &QComboBox::currentIndexChanged, this, &FindingsPage::refresh);
    connect(search_, &QLineEdit::textChanged, this, &FindingsPage::refresh);
    connect(path_, &QLineEdit::textChanged, this, &FindingsPage::refresh);
    connect(navigate_, &QPushButton::clicked, this, &FindingsPage::onFindingActivated);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &FindingsPage::onFindingActivated);
    connect(table_, &QTableWidget::itemSelectionChanged, this,
            [this] { navigate_->setEnabled(table_->currentRow() >= 0); });
    connect(reanalyze_, &QPushButton::clicked, &session_, &Session::requestAnalysis);
    connect(&session_, &Session::resultChanged, this, &FindingsPage::refresh);
    connect(&session_, &Session::stateChanged, this, [this] {
        reanalyze_->setEnabled(session_.hasInput() && session_.draftValid() && !session_.busy());
    });
    refresh();
}

void FindingsPage::refresh() {
    const int previousRow = table_->currentRow();
    const auto previousId = previousRow >= 0 && previousRow < static_cast<int>(visible_.size())
                                ? visible_[previousRow].id
                                : std::string{};
    const QSignalBlocker blocker(table_);
    table_->setRowCount(0);
    visible_.clear();
    code_->setSourceText({});
    explanation_->clear();
    navigate_->setEnabled(false);
    reanalyze_->setEnabled(session_.hasInput() && session_.draftValid() && !session_.busy());
    const auto snapshot = session_.snapshot();
    if (!snapshot) {
        count_->setText(tr("Немає результату аналізу."));
        return;
    }
    xray::analysis::FindingFilter filter;
    if (rule_->currentIndex() > 0)
        filter.rule = static_cast<xray::analysis::RuleId>(rule_->currentIndex() - 1);
    filter.textContains = search_->text().toUtf8().toStdString();
    filter.pathContains = path_->text().toUtf8().toStdString();
    visible_ = xray::analysis::filterFindings(*snapshot, filter);
    const auto index = functionIndex(*snapshot);
    for (const auto& finding : visible_) {
        const auto* fn = index.at(finding.entityId);
        const int row = table_->rowCount();
        table_->insertRow(row);
        const QStringList columns{ruleName(static_cast<int>(finding.ruleId)),
                                  text(fn->describe()),
                                  text(fn->relativePath) + ":" + QString::number(fn->range.startLine),
                                  QString::number(finding.observedValue),
                                  QString::number(finding.threshold),
                                  validityName(finding.validity)};
        for (int col = 0; col < columns.size(); ++col)
            table_->setItem(row, col, new QTableWidgetItem(columns[col]));
        if (finding.id == previousId) {
            table_->setCurrentCell(row, 0);
            table_->selectRow(row);
            navigate_->setEnabled(true);
        }
    }
    fitColumns(table_);
    count_->setText(tr("Показано %1 із %2 порушень. %3")
                        .arg(visible_.size())
                        .arg(snapshot->findings.size())
                        .arg(snapshot->findings.empty() ? tr("Порушень немає. Врахуйте покриття аналізу.")
                                                        : tr("Виберіть порушення для переходу до коду.")));
    if (visible_.empty() && !snapshot->findings.empty())
        count_->setText(tr("Збігів немає. Очистьте пошук/шлях або виберіть «Усі правила»."));
}

void FindingsPage::onFindingActivated() {
    const int row = table_->currentRow();
    const auto snapshot = session_.snapshot();
    if (!snapshot || row < 0 || row >= static_cast<int>(visible_.size()))
        return;
    const auto& finding = visible_[row];
    const auto index = functionIndex(*snapshot);
    const auto found = index.find(finding.entityId);
    if (found == index.end())
        return;
    const auto& fn = *found->second;
    explanation_->setText(text(finding.explanation) + "\n" + validityName(finding.validity));
    std::shared_ptr<const std::string> bytes;
    if (snapshot->parsed && snapshot->parsed->source)
        for (const auto& file : snapshot->parsed->source->files)
            if (file.relativePath == fn.relativePath) {
                bytes = file.bytes;
                break;
            }
    if (!bytes) {
        code_->setSourceText(tr("Вихідний код відсутній у знімку."));
        return;
    }
    QString source = text(*bytes);
    if (source.startsWith(QChar(0xfeff)))
        source.remove(0, 1);
    source.replace("\r\n", "\n");
    code_->setSourceText(source);
    const auto& lines = code_->sourceLineStarts();
    if (fn.range.startLine > 0 && fn.range.startLine <= fn.range.endLine &&
        fn.range.endLine <= lines.size()) {
        QTextCursor cursor(code_->document());
        cursor.setPosition(lines[fn.range.startLine - 1]);
        code_->setTextCursor(cursor);
        code_->centerCursor();
        QTextEdit::ExtraSelection highlight;
        highlight.cursor = cursor;
        const int end = fn.range.endLine < lines.size() ? lines[fn.range.endLine] - 1 : source.size();
        highlight.cursor.setPosition(end, QTextCursor::KeepAnchor);
        highlight.format.setBackground(QApplication::palette().highlight());
        highlight.format.setProperty(QTextFormat::FullWidthSelection, true);
        code_->setExtraSelections({highlight});
    }
    emit codeActivated(snapshot, text(fn.id));
}

bool FindingsPage::eventFilter(QObject* watched, QEvent* event) {
    if (watched == code_ &&
        (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)) {
        auto selections = code_->extraSelections();
        for (auto& selection : selections)
            selection.format.setBackground(QApplication::palette().highlight());
        code_->setExtraSelections(selections);
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace xray::ui::analysis_pages
