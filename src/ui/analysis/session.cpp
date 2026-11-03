#include "ui/analysis/session.hpp"

namespace xray::ui::analysis_pages {
namespace a = xray::analysis;
namespace {
QString errorMessage(const Error& error) {
    QString message = QString::fromStdString(error.message);
    if (error.path)
        message += "\n" + QString::fromStdString(*error.path);
    if (error.rule)
        message += "\n" + QString::fromStdString(*error.rule);
    return message;
}
} // namespace
Session::Session(QObject* parent) : QObject(parent), profiles_{a::AnalysisProfile{}} {
}

std::filesystem::path filePath(const QString& path) {
#ifdef _WIN32
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toUtf8().toStdString());
#endif
}

bool Session::current() const {
    return snapshot_ && draftValid_ && input_ && snapshot_->parsed == input_ &&
           a::isAnalysisCurrent(*snapshot_, profile());
}

void Session::reportMessage(QString message) {
    message_ = std::move(message);
    emit stateChanged();
}

bool Session::append(Result<a::AnalysisProfile> result) {
    if (const auto* error = std::get_if<Error>(&result)) {
        reportMessage(errorMessage(*error));
        return false;
    }
    auto* value = std::get_if<a::AnalysisProfile>(&result);
    if (!value) {
        reportMessage(tr("Операцію скасовано."));
        return false;
    }
    profiles_.push_back(std::move(*value));
    selected_ = static_cast<int>(profiles_.size()) - 1;
    draftValid_ = true;
    message_ = tr("Профіль відкрито. Натисніть «Зберегти», щоб записати його у файл.");
    emit profileChanged();
    emit stateChanged();
    return true;
}

bool Session::createProfile(const QString& name) {
    return append(a::duplicateProfile(a::AnalysisProfile{}, name.trimmed().toUtf8().toStdString()));
}

bool Session::duplicateProfile(const QString& name) {
    if (!draftValid_) {
        reportMessage(tr("Спочатку виправте пороги правил."));
        return false;
    }
    return append(a::duplicateProfile(profile(), name.trimmed().toUtf8().toStdString()));
}

bool Session::importProfile(const std::filesystem::path& path) {
    auto loaded = a::loadProfile(path);
    if (auto* value = std::get_if<a::AnalysisProfile>(&loaded)) {
        for (const auto& existing : profiles_)
            if (existing.id == value->id) {
                loaded = a::duplicateProfile(*value, value->name);
                break;
            }
    }
    return append(std::move(loaded));
}

bool Session::saveProfile(const std::filesystem::path& path) {
    if (!draftValid_) {
        reportMessage(tr("Профіль не збережено: виправте пороги правил."));
        return false;
    }
    auto result = a::saveProfile(profile(), path);
    if (auto* error = std::get_if<Error>(&result)) {
        reportMessage(errorMessage(*error));
        return false;
    }
    if (!std::holds_alternative<std::filesystem::path>(result)) {
        reportMessage(tr("Збереження скасовано."));
        return false;
    }
    reportMessage(tr("Профіль збережено."));
    return true;
}

void Session::selectProfile(int index) {
    if (index < 0 || index >= static_cast<int>(profiles_.size()) || index == selected_)
        return;
    selected_ = index;
    draftValid_ = true;
    message_.clear();
    emit profileChanged();
    emit stateChanged();
}

void Session::editRules(const std::array<bool, 3>& enabled, const std::array<std::int64_t, 3>& limits) {
    auto candidate = profile();
    candidate.enabledRules = enabled;
    candidate.longFunctionLimit = limits[0];
    candidate.nestingLimit = limits[1];
    candidate.branchLimit = limits[2];
    if (a::validateProfile(candidate)) {
        setDraftValid(false);
        return;
    }
    profiles_[selected_] = std::move(candidate);
    draftValid_ = true;
    emit stateChanged();
}

void Session::setDraftValid(bool valid) {
    draftValid_ = valid;
    emit stateChanged();
}

void Session::setInput(std::shared_ptr<const code::ParsedSnapshot> input) {
    input_ = std::move(input);
    emit stateChanged();
}

void Session::setBusy(bool busy) {
    busy_ = busy;
    emit stateChanged();
}

void Session::present(Result<a::AnalysisSnapshot> result) {
    if (auto* value = std::get_if<a::AnalysisSnapshot>(&result)) {
        snapshot_ = std::make_shared<const a::AnalysisSnapshot>(std::move(*value));
        message_ =
            snapshot_->metrics.empty() ? tr("Аналіз завершено: функцій немає.") : tr("Аналіз завершено.");
        emit resultChanged();
    } else if (auto* error = std::get_if<Error>(&result)) {
        message_ = tr("Помилка: %1").arg(errorMessage(*error));
    } else
        message_ = tr("Операцію скасовано. Попередній результат збережено.");
    emit stateChanged();
}

void Session::requestAnalysis() {
    if (!busy_ && input_ && draftValid_)
        emit analysisRequested(profile());
}
} // namespace xray::ui::analysis_pages
