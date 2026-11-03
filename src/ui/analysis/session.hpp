#pragma once

#include "analysis/profiles.hpp"
#include <QObject>
#include <QString>

namespace xray::ui::analysis_pages {
// GUI session state only. The app coordinator owns asynchronous work.
class Session final : public QObject {
    Q_OBJECT
  public:
    explicit Session(QObject* parent = nullptr);
    const std::vector<xray::analysis::AnalysisProfile>& profiles() const { return profiles_; }
    const xray::analysis::AnalysisProfile& profile() const { return profiles_.at(selected_); }
    int selected() const { return selected_; }
    std::shared_ptr<const xray::analysis::AnalysisSnapshot> snapshot() const { return snapshot_; }
    bool busy() const { return busy_; }
    bool hasInput() const { return bool(input_); }
    bool draftValid() const { return draftValid_; }
    bool current() const;
    QString message() const { return message_; }
    bool createProfile(const QString& name);
    bool duplicateProfile(const QString& name);
    bool importProfile(const std::filesystem::path& path);
    bool saveProfile(const std::filesystem::path& path);
    void selectProfile(int index);
    void editRules(const std::array<bool, 3>& enabled, const std::array<std::int64_t, 3>& limits);
    void setDraftValid(bool valid);
    void setInput(std::shared_ptr<const code::ParsedSnapshot> input);
    void setBusy(bool busy);
    void present(xray::Result<xray::analysis::AnalysisSnapshot> result);
    void reportMessage(QString message);
    void requestAnalysis();
  signals:
    void resultChanged();
    void profileChanged();
    void stateChanged();
    void analysisRequested(xray::analysis::AnalysisProfile profile);

  private:
    bool append(xray::Result<xray::analysis::AnalysisProfile> result);
    std::vector<xray::analysis::AnalysisProfile> profiles_;
    int selected_ = 0;
    bool busy_ = false, draftValid_ = true;
    std::shared_ptr<const code::ParsedSnapshot> input_;
    QString message_;
    std::shared_ptr<const xray::analysis::AnalysisSnapshot> snapshot_;
};
std::filesystem::path filePath(const QString& path);
} // namespace xray::ui::analysis_pages
