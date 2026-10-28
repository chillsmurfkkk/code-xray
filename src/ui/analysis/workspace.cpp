#include "ui/analysis/workspace.hpp"
#include "ui/analysis/profiles_page.hpp"
#include "ui/analysis/metrics_page.hpp"
#include "ui/analysis/rules_page.hpp"
#include "ui/analysis/findings_page.hpp"
#include "ui/analysis/presentation.hpp"
#include "analysis/engine.hpp"
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QProgressBar>

namespace xray::ui::analysis_pages {
Workspace::Workspace(Session& session, QWidget* parent) : QWidget(parent), session_(session) {
    setObjectName("analysisWorkspace");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(12);
    title_ = new QLabel(tr("Метрики"));
    title_->setObjectName("panelTitle");
    auto font = title_->font();
    font.setPointSizeF(font.pointSizeF() + 4);
    font.setBold(true);
    title_->setFont(font);
    layout->addWidget(title_);
    context_ = new QLabel(tr("Відкрийте папку у розділі «Код → Структура»."));
    context_->setObjectName("sourceContext");
    context_->setWordWrap(true);
    context_->setTextFormat(Qt::PlainText);
    layout->addWidget(context_);
    status_ = new QLabel;
    status_->setObjectName("analysisStatus");
    status_->setWordWrap(true);
    status_->setTextFormat(Qt::PlainText);
    coverage_ = new QLabel;
    coverage_->setObjectName("coverageStatus");
    coverage_->setWordWrap(true);
    coverage_->setTextFormat(Qt::PlainText);
    message_ = new QLabel;
    message_->setObjectName("sessionMessage");
    message_->setWordWrap(true);
    message_->setTextFormat(Qt::PlainText);
    layout->addWidget(status_);
    layout->addWidget(coverage_);
    layout->addWidget(message_);
    auto* activity = new QHBoxLayout;
    progress_ = new QProgressBar;
    progress_->setObjectName("analysisProgress");
    progress_->setRange(0, 0);
    cancel_ = new QPushButton(tr("Скасувати аналіз"));
    cancel_->setObjectName("cancelAnalysis");
    cancel_->setProperty("role", "primaryAction");
    activity->addWidget(progress_, 1);
    activity->addWidget(cancel_);
    layout->addLayout(activity);
    connect(cancel_, &QPushButton::clicked, this, &Workspace::cancelRequested);
    auto* panel = new QFrame;
    panel->setObjectName("workPanel");
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    pages_ = new QStackedWidget;
    pages_->setObjectName("analysisPages");
    pages_->addWidget(new ProfilesPage(session_));
    pages_->addWidget(new MetricsPage(session_));
    pages_->addWidget(new RulesPage(session_));
    auto* findings = new FindingsPage(session_);
    pages_->addWidget(findings);
    for (int i = 0; i < pages_->count(); ++i) {
        pages_->widget(i)->layout()->setContentsMargins(16, 16, 16, 16);
        pages_->widget(i)->layout()->setSpacing(12);
    }
    pages_->setCurrentIndex(1);
    connect(findings, &FindingsPage::codeActivated, this, &Workspace::codeActivated);
    panelLayout->addWidget(pages_);
    layout->addWidget(panel, 1);
    connect(&session_, &Session::stateChanged, this, &Workspace::refreshStatus);
    refreshStatus();
}

void Workspace::refreshStatus() {
    progress_->setVisible(session_.busy());
    cancel_->setVisible(session_.busy());
    const auto snapshot = session_.snapshot();
    QString state = !snapshot            ? tr("Немає результату")
                    : session_.current() ? tr("Результат актуальний")
                                         : tr("Результат застарілий — повторіть аналіз");
    if (!session_.draftValid())
        state += tr(" · пороги містять помилку");
    if (session_.busy())
        state += tr(" · триває обчислення");
    status_->setText(tr("Профіль: %1 · %2").arg(text(session_.profile().name), state));
    message_->setText(session_.message());
    coverage_->clear();
    if (!snapshot || !snapshot->parsed)
        return;
    const auto counts = xray::analysis::summarizeCoverage(*snapshot->parsed);
    QString details =
        tr("Покриття: %1 · доступних: %2 · лише синтаксично: %3 · невідомих: %4 · незастосовних: %5 · "
           "пропущено: %6")
            .arg(snapshot->coverage.completeness == Completeness::complete ? tr("повне") : tr("часткове"))
            .arg(counts.valid)
            .arg(counts.syntacticOnly)
            .arg(counts.unavailable)
            .arg(counts.notApplicable)
            .arg(snapshot->coverage.skippedElements);
    coverage_->setText(details);
    QStringList diagnostics;
    for (const auto& diagnostic : snapshot->coverage.diagnostics)
        diagnostics << (diagnostic.path ? text(*diagnostic.path) + ": " : QString{}) +
                           text(diagnostic.message);
    coverage_->setToolTip(diagnostics.join('\n'));
}

void Workspace::setCurrentPage(int index) {
    if (index >= 0 && index < pages_->count()) {
        pages_->setCurrentIndex(index);
        title_->setText(QStringList{tr("Профілі"), tr("Метрики"), tr("Правила"), tr("Порушення")}[index]);
    }
}

int Workspace::currentPage() const {
    return pages_->currentIndex();
}

void Workspace::setContext(const QString& context) {
    context_->setText(context);
}

void Workspace::setProgress(const Progress& progress) {
    progress_->setRange(0, progress.total && *progress.total ? 100 : 0);
    if (progress.total && *progress.total)
        progress_->setValue(static_cast<int>(100.0L * progress.completed / *progress.total));
}

} // namespace xray::ui::analysis_pages
