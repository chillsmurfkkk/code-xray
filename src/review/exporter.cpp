// exporter.cpp — D20, D21, D22, D23, D24, exportReport
#include "review/exporter.hpp"
#include "review/api.hpp"

#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string_view>

namespace xray::review {

// ─────────────────────────────────────────────────────────────
// D21  escapeHtmlText
// Екранувати текст для HTML, уникаючи подвійного екранування вже екранованих сутностей.
// ─────────────────────────────────────────────────────────────
std::string escapeHtmlText(const std::string& input)
{
    std::string out;
    out.reserve(input.size() * 11 / 10);

    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '&') {
            // Перевіряємо чи це вже екранована сутність (уникнення подвійного екранування)
            std::string_view rest(input.data() + i, input.size() - i);
            if (rest.rfind("&amp;", 0) == 0 ||
                rest.rfind("&lt;", 0) == 0 ||
                rest.rfind("&gt;", 0) == 0 ||
                rest.rfind("&quot;", 0) == 0 ||
                rest.rfind("&#39;", 0) == 0) {
                out += '&'; // залишаємо як є
            } else {
                out += "&amp;";
            }
        } else if (c == '<') {
            out += "&lt;";
        } else if (c == '>') {
            out += "&gt;";
        } else if (c == '"') {
            out += "&quot;";
        } else if (c == '\'') {
            out += "&#39;";
        } else {
            out += c;
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────
// D24  writeReportSafely
// Безпечний запис у файл через тимчасовий файл з перевіркою дозволу на перезапис.
// ─────────────────────────────────────────────────────────────
bool writeReportSafely(const std::filesystem::path& destination,
                       const std::string& content,
                       bool overwriteConfirmed,
                       std::uintmax_t* bytesWritten)
{
    if (destination.empty()) return false;

    // Якщо файл існує і перезапис не підтверджено — відхиляємо
    std::error_code ec;
    if (std::filesystem::exists(destination, ec) && !overwriteConfirmed) {
        return false;
    }

    auto parent = destination.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) return false;
    }

    auto tmpPath = destination;
    tmpPath += ".tmp";

    {
        std::ofstream out(tmpPath, std::ios::binary);
        if (!out.is_open()) return false;
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out.good()) {
            out.close();
            std::filesystem::remove(tmpPath, ec);
            return false;
        }
    }

    // Атомарна заміна / перейменування
    std::filesystem::rename(tmpPath, destination, ec);
    if (ec) {
        // На Windows, якщо destination вже існує, rename може дати помилку
        std::filesystem::copy_file(tmpPath, destination,
            std::filesystem::copy_options::overwrite_existing, ec);
        std::error_code removeEc;
        std::filesystem::remove(tmpPath, removeEc);
        if (ec) return false;
    }

    if (bytesWritten) {
        *bytesWritten = content.size();
    }
    return true;
}

// ─────────────────────────────────────────────────────────────
// D20  buildReportModel
// Перетворення доменних результатів на модель звіту
// ─────────────────────────────────────────────────────────────
ReportModel buildReportModel(const ComparisonReport& report)
{
    ReportModel model;
    model.baseId = report.baseId;
    model.targetId = report.targetId;
    model.analysisFingerprint = report.analysisFingerprint;
    model.metricSchemaVersion = report.metricSchemaVersion;
    model.summary = summarizeComparison(report.items);
    model.totalItems = report.items.size();

    auto kindToString = [](ReviewKind k) {
        switch (k) {
        case ReviewKind::added:     return "added";
        case ReviewKind::removed:   return "removed";
        case ReviewKind::changed:   return "changed";
        case ReviewKind::unchanged: return "unchanged";
        case ReviewKind::unmatched: return "unmatched";
        }
        return "unknown";
    };

    auto confidenceToString = [](Confidence c) {
        switch (c) {
        case Confidence::exact:      return "exact";
        case Confidence::ambiguous:  return "ambiguous";
        case Confidence::incomplete: return "incomplete";
        }
        return "unknown";
    };

    for (const auto& item : report.items) {
        ReportModelRow row;
        row.kind = kindToString(item.kind);
        row.entityKey = item.entityKey;
        row.confidence = confidenceToString(item.confidence);
        row.explanation = item.explanation;
        row.path = item.sourceRefs.empty() ? "" : item.sourceRefs.front().relativePath;

        std::ostringstream metricsOut;
        for (std::size_t i = 0; i < item.metrics.size(); ++i) {
            const auto& m = item.metrics[i];
            if (i > 0) metricsOut << ", ";
            if (m.delta.has_value()) {
                long long d = static_cast<long long>(*m.delta);
                metricsOut << (d > 0 ? "+" : "") << d;
            } else {
                metricsOut << "N/A";
            }
        }
        row.metricsSummary = metricsOut.str();

        std::ostringstream findingsOut;
        for (std::size_t i = 0; i < item.findings.size(); ++i) {
            const auto& f = item.findings[i];
            if (i > 0) findingsOut << ", ";
            switch (f.state) {
            case FindingState::new_finding:       findingsOut << "new"; break;
            case FindingState::resolved:          findingsOut << "resolved"; break;
            case FindingState::existing:          findingsOut << "existing"; break;
            case FindingState::removed_with_code: findingsOut << "removed_code"; break;
            case FindingState::unknown:           findingsOut << "unknown"; break;
            }
        }
        row.findingsSummary = findingsOut.str();

        model.rows.push_back(std::move(row));
    }

    return model;
}

// ─────────────────────────────────────────────────────────────
// D22  HtmlExporter::write
// ─────────────────────────────────────────────────────────────
bool HtmlExporter::write(const ComparisonReport& report, const ExportOptions& options)
{
    ReportModel model = buildReportModel(report);

    std::ostringstream html;
    html << "<!DOCTYPE html>\n"
         << "<html lang=\"en\">\n"
         << "<head>\n"
         << "  <meta charset=\"UTF-8\">\n"
         << "  <title>Code Xray Comparison Report</title>\n"
         << "  <style>\n"
         << "    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 2rem; background: #fafafa; color: #333; }\n"
         << "    h1 { color: #1a1a1a; }\n"
         << "    .meta-box { background: #fff; padding: 1rem; border-radius: 6px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); margin-bottom: 1.5rem; }\n"
         << "    .meta-row { margin: 0.25rem 0; font-size: 0.95rem; }\n"
         << "    .badge { display: inline-block; padding: 0.2rem 0.5rem; border-radius: 4px; font-size: 0.8rem; font-weight: bold; }\n"
         << "    .badge-added { background: #d4edda; color: #155724; }\n"
         << "    .badge-removed { background: #f8d7da; color: #721c24; }\n"
         << "    .badge-changed { background: #fff3cd; color: #856404; }\n"
         << "    .badge-unmatched { background: #e2e3e5; color: #383d41; }\n"
         << "    .badge-unchanged { background: #e9ecef; color: #495057; }\n"
         << "    table { width: 100%; border-collapse: collapse; background: #fff; box-shadow: 0 1px 3px rgba(0,0,0,0.1); border-radius: 6px; overflow: hidden; }\n"
         << "    th, td { padding: 0.75rem 1rem; text-align: left; border-bottom: 1px solid #eee; font-size: 0.9rem; }\n"
         << "    th { background: #f1f3f5; font-weight: 600; }\n"
         << "    tr:hover { background: #f8f9fa; }\n"
         << "    code { font-family: Consolas, monospace; background: #f1f1f1; padding: 0.1rem 0.3rem; border-radius: 3px; }\n"
         << "  </style>\n"
         << "</head>\n"
         << "<body>\n"
         << "  <h1>Code Xray Comparison Report</h1>\n"
         << "  <div class=\"meta-box\">\n"
         << "    <div class=\"meta-row\"><strong>Base Snapshot:</strong> " << escapeHtmlText(model.baseId) << "</div>\n"
         << "    <div class=\"meta-row\"><strong>Target Snapshot:</strong> " << escapeHtmlText(model.targetId) << "</div>\n"
         << "    <div class=\"meta-row\"><strong>Analysis Fingerprint:</strong> <code>" << escapeHtmlText(model.analysisFingerprint) << "</code></div>\n"
         << "    <div class=\"meta-row\"><strong>Metric Schema:</strong> " << escapeHtmlText(model.metricSchemaVersion) << "</div>\n"
         << "    <div class=\"meta-row\"><strong>Summary:</strong> " << escapeHtmlText(model.summary) << "</div>\n"
         << "  </div>\n"
         << "  <table>\n"
         << "    <thead>\n"
         << "      <tr>\n"
         << "        <th>Kind</th>\n"
         << "        <th>Entity</th>\n"
         << "        <th>Path</th>\n"
         << "        <th>Metric Delta</th>\n"
         << "        <th>Findings</th>\n"
         << "        <th>Confidence</th>\n"
         << "        <th>Explanation</th>\n"
         << "      </tr>\n"
         << "    </thead>\n"
         << "    <tbody>\n";

    for (const auto& row : model.rows) {
        html << "      <tr>\n"
             << "        <td><span class=\"badge badge-" << escapeHtmlText(row.kind) << "\">"
             << escapeHtmlText(row.kind) << "</span></td>\n"
             << "        <td><code>" << escapeHtmlText(row.entityKey) << "</code></td>\n"
             << "        <td>" << escapeHtmlText(row.path) << "</td>\n"
             << "        <td>" << escapeHtmlText(row.metricsSummary) << "</td>\n"
             << "        <td>" << escapeHtmlText(row.findingsSummary) << "</td>\n"
             << "        <td>" << escapeHtmlText(row.confidence) << "</td>\n"
             << "        <td>" << escapeHtmlText(row.explanation) << "</td>\n"
             << "      </tr>\n";
    }

    html << "    </tbody>\n"
         << "  </table>\n"
         << "</body>\n"
         << "</html>\n";

    return writeReportSafely(options.destination, html.str(), options.overwriteConfirmed);
}

// ─────────────────────────────────────────────────────────────
// D23  JsonExporter::write
// ─────────────────────────────────────────────────────────────
bool JsonExporter::write(const ComparisonReport& report, const ExportOptions& options)
{
    ReportModel model = buildReportModel(report);

    std::ostringstream json;
    json << "{\n"
         << "  \"schemaVersion\": \"comparison-v1\",\n"
         << "  \"baseId\": \"" << model.baseId << "\",\n"
         << "  \"targetId\": \"" << model.targetId << "\",\n"
         << "  \"analysisFingerprint\": \"" << model.analysisFingerprint << "\",\n"
         << "  \"metricSchemaVersion\": \"" << model.metricSchemaVersion << "\",\n"
         << "  \"summary\": \"" << model.summary << "\",\n"
         << "  \"totalItems\": " << model.totalItems << ",\n"
         << "  \"items\": [\n";

    for (std::size_t i = 0; i < report.items.size(); ++i) {
        const auto& item = report.items[i];
        json << "    {\n"
             << "      \"id\": \"" << item.id << "\",\n"
             << "      \"entityKey\": \"" << item.entityKey << "\",\n"
             << "      \"explanation\": \"" << item.explanation << "\",\n"
             << "      \"metrics\": [";

        for (std::size_t mi = 0; mi < item.metrics.size(); ++mi) {
            const auto& m = item.metrics[mi];
            if (mi > 0) json << ", ";
            json << "{\"metricId\": " << static_cast<int>(m.metricId) << ", ";
            if (m.before.has_value()) json << "\"before\": " << *m.before << ", ";
            else json << "\"before\": null, ";
            if (m.after.has_value()) json << "\"after\": " << *m.after << ", ";
            else json << "\"after\": null, ";
            if (m.delta.has_value()) json << "\"delta\": " << *m.delta << "}";
            else json << "\"delta\": null}";
        }
        json << "],\n"
             << "      \"findings\": [";
        for (std::size_t fi = 0; fi < item.findings.size(); ++fi) {
            const auto& f = item.findings[fi];
            if (fi > 0) json << ", ";
            json << "{\"ruleId\": " << static_cast<int>(f.ruleId) << ", \"state\": "
                 << static_cast<int>(f.state) << "}";
        }
        json << "]\n    }" << (i + 1 < report.items.size() ? ",\n" : "\n");
    }

    json << "  ]\n}\n";

    return writeReportSafely(options.destination, json.str(), options.overwriteConfirmed);
}

// ─────────────────────────────────────────────────────────────
// exportReport — головний фасад експорту (Dynamic polymorphism)
// ─────────────────────────────────────────────────────────────
Result<ExportReceipt> exportReport(const ComparisonReport& report,
                                   const ExportOptions& options,
                                   const JobContext& ctx)
{
    if (options.destination.empty()) {
        return Error{ErrorCode::invalid_input, "exportReport: destination path cannot be empty."};
    }

    if (ctx.isCancelled()) return Cancelled{};

    ctx.report("Exporting report", 0, 1);

    std::unique_ptr<ReportExporter> exporter;
    if (options.format == ExportFormat::html) {
        exporter = std::make_unique<HtmlExporter>();
    } else {
        exporter = std::make_unique<JsonExporter>();
    }

    bool success = exporter->write(report, options);
    if (!success) {
        return Error{
            ErrorCode::export_error,
            "exportReport: failed to write report to " + options.destination.string() +
            " (check permissions or overwriteConfirmed flag)"
        };
    }

    ctx.report("Done", 1, 1);

    std::error_code ec;
    auto size = std::filesystem::file_size(options.destination, ec);
    return ExportReceipt{options.destination, ec ? 0 : size};
}

} // namespace xray::review
