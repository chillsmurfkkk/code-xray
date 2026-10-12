#pragma once

#include "review/types.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace xray::review {

// D21 — escape HTML characters (&, <, >, ", ') safely
std::string escapeHtmlText(const std::string& input);

// D24 — safely write file via atomic rename with overwrite confirmation check
bool writeReportSafely(const std::filesystem::path& destination,
                       const std::string& content,
                       bool overwriteConfirmed,
                       std::uintmax_t* bytesWritten = nullptr);

// D20 — intermediate data model for report rendering
struct ReportModelRow {
    std::string kind;
    std::string entityKey;
    std::string path;
    std::string metricsSummary;
    std::string findingsSummary;
    std::string explanation;
    std::string confidence;
};

struct ReportModel {
    std::string baseId;
    std::string targetId;
    std::string analysisFingerprint;
    std::string metricSchemaVersion;
    std::string summary;
    std::size_t totalItems = 0;
    std::vector<ReportModelRow> rows;
};

ReportModel buildReportModel(const ComparisonReport& report);

// Abstract exporter interface (Dynamic polymorphism 2)
class ReportExporter {
public:
    virtual ~ReportExporter() = default;
    virtual bool write(const ComparisonReport& report, const ExportOptions& options) = 0;
};

// D22 — HTML exporter
class HtmlExporter : public ReportExporter {
public:
    bool write(const ComparisonReport& report, const ExportOptions& options) override;
};

// D23 — JSON exporter
class JsonExporter : public ReportExporter {
public:
    bool write(const ComparisonReport& report, const ExportOptions& options) override;
};

} // namespace xray::review
