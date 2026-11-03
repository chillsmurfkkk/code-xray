#include "app/analysis_controller.hpp"
#include "analysis/api.hpp"
#include <QThread>
#include <exception>
#include <QMetaObject>

namespace xray::app {
AnalysisController::AnalysisController(QObject* parent) : QObject(parent) {
    connect(&session_, &ui::analysis_pages::Session::analysisRequested, this, &AnalysisController::start);
}
AnalysisController::~AnalysisController() {
    stop_.request_stop();
    if (thread_)
        thread_->wait();
}
void AnalysisController::setInput(std::shared_ptr<const code::ParsedSnapshot> input) {
    ++generation_;
    input_ = std::move(input);
    session_.setInput(input_);
    pending_ = bool(input_);
    if (thread_)
        stop_.request_stop();
    else if (pending_)
        start(session_.profile());
}
void AnalysisController::start(analysis::AnalysisProfile profile) {
    if (thread_ || !input_ || !session_.draftValid())
        return;
    pending_ = false;
    const auto id = ++generation_;
    stop_ = std::stop_source{};
    const auto token = stop_.get_token();
    auto result = std::make_shared<Result<analysis::AnalysisSnapshot>>(Cancelled{});
    session_.setBusy(true);
    session_.reportMessage(tr("Аналізуємо функції…"));
    emit progressChanged(Progress{id, "analyze-functions", 0, {}});
    thread_ = QThread::create([this, input = input_, profile = std::move(profile), token, result, id] {
        try {
            JobContext job{id, token, [this, id](const Progress& progress) {
                               QMetaObject::invokeMethod(
                                   this,
                                   [this, id, progress] {
                                       if (id == generation_ && thread_)
                                           emit progressChanged(progress);
                                   },
                                   Qt::QueuedConnection);
                           }};
            *result = analysis::analyze(input, profile, job);
            if (token.stop_requested())
                *result = Cancelled{};
        } catch (const std::exception& error) {
            *result = Error{ErrorCode::invalid_input, error.what(), {}, {}, {}};
        } catch (...) {
            *result = Error{ErrorCode::invalid_input, "Unexpected analysis failure", {}, {}, {}};
        }
    });
    thread_->setParent(this);
    connect(thread_, &QThread::finished, this, [this, result, id] {
        thread_ = nullptr;
        if (stop_.stop_requested())
            *result = Cancelled{};
        if (id == generation_)
            session_.present(std::move(*result));
        else if (!pending_)
            session_.reportMessage(tr("Аналіз скасовано."));
        session_.setBusy(false);
        if (pending_ && input_)
            start(session_.profile());
    });
    connect(thread_, &QThread::finished, thread_, &QObject::deleteLater);
    thread_->start();
}
void AnalysisController::cancel() {
    if (thread_) {
        pending_ = false;
        stop_.request_stop();
        session_.reportMessage(tr("Скасовуємо аналіз…"));
    }
}
} // namespace xray::app
