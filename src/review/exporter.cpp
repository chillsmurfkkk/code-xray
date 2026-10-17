// exporter.cpp — D20, D21, D22, D23, D24, exportReport
#include "review/exporter.hpp"
#include "review/api.hpp"

#include <chrono>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <string_view>

#include <nlohmann/json.hpp>
#include <inja/inja.hpp>

namespace xray::review {

// ─────────────────────────────────────────────────────────────
// D21  escapeHtmlText
// ─────────────────────────────────────────────────────────────
std::string escapeHtmlText(const std::string& input)
{
    std::string out;
    out.reserve(input.size() * 11 / 10);

    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '&') {
            std::string_view rest(input.data() + i, input.size() - i);
            if (rest.rfind("&amp;", 0) == 0 ||
                rest.rfind("&lt;", 0) == 0 ||
                rest.rfind("&gt;", 0) == 0 ||
                rest.rfind("&quot;", 0) == 0 ||
                rest.rfind("&#39;", 0) == 0) {
                out += '&';
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
// ─────────────────────────────────────────────────────────────
bool writeReportSafely(const std::filesystem::path& destination,
                       const std::string& content,
                       bool overwriteConfirmed,
                       std::uintmax_t* bytesWritten,
                       const JobContext* ctx)
{
    if (destination.empty()) return false;
    if (ctx && ctx->isCancelled()) return false;

    std::error_code ec;
    bool destExists = std::filesystem::exists(destination, ec);
    if (destExists && !overwriteConfirmed) {
        return false;
    }

    if (ctx && ctx->isCancelled()) return false;

    auto parent = destination.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) return false;
    }

    if (ctx && ctx->isCancelled()) return false;

    // Унікальне ім'я тимчасового файлу, щоб не перезаписати чужий .tmp
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    static std::mt19937_64 rng(1337);
    auto rnd = rng();
    auto tmpPath = destination;
    tmpPath += ".tmp." + std::to_string(now) + "." + std::to_string(rnd);

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

    // Перевірка скасування перед будь-якою зміною оригінального файлу
    if (ctx && ctx->isCancelled()) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }

    // Безпечна заміна зі збереженням старого файлу при помилці або скасуванні
    if (destExists) {
        auto bakPath = destination;
        bakPath += ".bak." + std::to_string(now) + "." + std::to_string(rnd);

        // Переміщуємо старий файл у бекап
        std::filesystem::rename(destination, bakPath, ec);
        if (ec) {
            // Якщо rename не спрацював, не чіпаємо оригінал
            std::filesystem::remove(tmpPath, ec);
            return false;
        }

        // Перевірка скасування безпосередньо перед переміщенням нового файлу
        if (ctx && ctx->isCancelled()) {
            std::error_code restoreEc;
            std::filesystem::rename(bakPath, destination, restoreEc);
            std::filesystem::remove(tmpPath, ec);
            return false;
        }

        // Переміщуємо новий файл на місце цільового
        std::filesystem::rename(tmpPath, destination, ec);
        if (ec) {
            // Помилка встановлення нового файлу — відновлюємо старий з бекапу!
            std::error_code restoreEc;
            std::filesystem::rename(bakPath, destination, restoreEc);
            std::filesystem::remove(tmpPath, ec);
            return false;
        }

        // Успішно замінено — видаляємо бекап
        std::filesystem::remove(bakPath, ec);
    } else {
        if (ctx && ctx->isCancelled()) {
            std::filesystem::remove(tmpPath, ec);
            return false;
        }
        std::filesystem::rename(tmpPath, destination, ec);
        if (ec) {
            std::filesystem::remove(tmpPath, ec);
            return false;
        }
    }

    if (bytesWritten) {
        *bytesWritten = content.size();
    }
    return true;
}

// ─────────────────────────────────────────────────────────────
// Допоміжні функції найменувань
// ─────────────────────────────────────────────────────────────
static std::string metricName(analysis::MetricId id) {
    switch (id) {
    case analysis::MetricId::function_lines: return "lines";
    case analysis::MetricId::max_nesting:   return "nesting";
    case analysis::MetricId::branch_count:  return "branches";
    }
    return "metric_" + std::to_string(static_cast<int>(id));
}

static std::string ruleName(analysis::RuleId id) {
    switch (id) {
    case analysis::RuleId::long_function: return "long_function";
    case analysis::RuleId::deep_nesting:  return "deep_nesting";
    case analysis::RuleId::many_branches: return "many_branches";
    }
    return "rule_" + std::to_string(static_cast<int>(id));
}

// ─────────────────────────────────────────────────────────────
// D20  buildReportModel
// ─────────────────────────────────────────────────────────────
ReportModel buildReportModel(const ComparisonReport& report, const JobContext* ctx)
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

    for (std::size_t itemIdx = 0; itemIdx < report.items.size(); ++itemIdx) {
        if (ctx && ctx->isCancelled()) {
            break;
        }
        const auto& item = report.items[itemIdx];
        ReportModelRow row;
        row.kind = kindToString(item.kind);
        row.entityKey = item.entityKey;
        row.confidence = confidenceToString(item.confidence);
        row.explanation = item.explanation;
        row.path = item.sourceRefs.empty() ? "" : item.sourceRefs.front().relativePath;

        std::ostringstream metricsOut;
        for (std::size_t i = 0; i < item.metrics.size(); ++i) {
            const auto& m = item.metrics[i];
            if (i > 0) metricsOut << "; ";
            metricsOut << metricName(m.metricId) << ": ";
            if (m.before.has_value() && m.after.has_value() && m.delta.has_value()) {
                metricsOut << *m.before << " -> " << *m.after << " ("
                           << (*m.delta >= 0 ? "+" : "") << *m.delta << ")";
            } else if (m.delta.has_value()) {
                metricsOut << (*m.delta >= 0 ? "+" : "") << *m.delta;
            } else if (m.after.has_value()) {
                metricsOut << "now " << *m.after;
            } else if (m.before.has_value()) {
                metricsOut << "was " << *m.before;
            } else {
                metricsOut << "unknown";
            }
        }
        row.metricsSummary = metricsOut.str();

        std::ostringstream findingsOut;
        for (std::size_t i = 0; i < item.findings.size(); ++i) {
            const auto& f = item.findings[i];
            if (i > 0) findingsOut << "; ";
            findingsOut << ruleName(f.ruleId) << ": ";
            switch (f.state) {
            case FindingState::new_finding:
                findingsOut << "new";
                break;
            case FindingState::resolved:
                findingsOut << "resolved";
                break;
            case FindingState::removed_with_code:
                findingsOut << "removed with code";
                break;
            case FindingState::existing:
                findingsOut << "existing";
                break;
            case FindingState::unknown:
                findingsOut << "unknown";
                break;
            }
        }
        row.findingsSummary = findingsOut.str();

        model.rows.push_back(std::move(row));
    }

    return model;
}

// ─────────────────────────────────────────────────────────────
// D22  HtmlExporter::write (Шаблонізація через inja з autoescape/escapeHtmlText)
// ─────────────────────────────────────────────────────────────
bool HtmlExporter::write(const ComparisonReport& report, const ExportOptions& options, const JobContext& ctx)
{
    if (ctx.isCancelled()) return false;
    ctx.report("Building report model", 10, 100);

    ReportModel model = buildReportModel(report, &ctx);
    if (ctx.isCancelled()) return false;

    ctx.report("Serializing data for HTML template", 30, 100);

    nlohmann::json data;
    data["baseId"] = escapeHtmlText(model.baseId);
    data["targetId"] = escapeHtmlText(model.targetId);
    data["analysisFingerprint"] = escapeHtmlText(model.analysisFingerprint);
    data["metricSchemaVersion"] = escapeHtmlText(model.metricSchemaVersion);
    data["summary"] = escapeHtmlText(model.summary);
    data["totalItems"] = model.totalItems;

    // Coverage у моделі звіту
    data["completeness"] = (report.coverage.completeness == Completeness::complete) ? "complete" : "partial";
    data["skippedElements"] = report.coverage.skippedElements;
    data["diagnosticsCount"] = report.coverage.diagnostics.size();

    nlohmann::json rows = nlohmann::json::array();
    for (std::size_t i = 0; i < model.rows.size(); ++i) {
        if ((i % 50 == 0) && ctx.isCancelled()) {
            return false;
        }
        const auto& row = model.rows[i];
        nlohmann::json r;
        r["kind"] = escapeHtmlText(row.kind);
        r["entityKey"] = escapeHtmlText(row.entityKey);
        r["path"] = escapeHtmlText(row.path);
        r["metricsSummary"] = escapeHtmlText(row.metricsSummary);
        r["findingsSummary"] = escapeHtmlText(row.findingsSummary);
        r["confidence"] = escapeHtmlText(row.confidence);
        r["explanation"] = escapeHtmlText(row.explanation);
        rows.push_back(r);
    }
    data["rows"] = rows;

    if (ctx.isCancelled()) return false;
    ctx.report("Rendering HTML template with inja", 60, 100);

    const std::string tmpl = R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Code Xray Comparison Report</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 2rem; background: #fafafa; color: #333; }
    h1 { color: #1a1a1a; }
    .meta-box { background: #fff; padding: 1rem; border-radius: 6px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); margin-bottom: 1.5rem; }
    .meta-row { margin: 0.25rem 0; font-size: 0.95rem; }
    .badge { display: inline-block; padding: 0.2rem 0.5rem; border-radius: 4px; font-size: 0.8rem; font-weight: bold; }
    .badge-added { background: #d4edda; color: #155724; }
    .badge-removed { background: #f8d7da; color: #721c24; }
    .badge-changed { background: #fff3cd; color: #856404; }
    .badge-unmatched { background: #e2e3e5; color: #383d41; }
    .badge-unchanged { background: #e9ecef; color: #495057; }
    table { width: 100%; border-collapse: collapse; background: #fff; box-shadow: 0 1px 3px rgba(0,0,0,0.1); border-radius: 6px; overflow: hidden; }
    th, td { padding: 0.75rem 1rem; text-align: left; border-bottom: 1px solid #eee; font-size: 0.9rem; }
    th { background: #f4f5f7; font-weight: 600; color: #555; }
    tr:hover { background: #f9f9f9; }
    code { font-family: 'Consolas', 'Monaco', monospace; background: #eee; padding: 0.1rem 0.3rem; border-radius: 3px; font-size: 0.85rem; }
  </style>
</head>
<body>
  <h1>Code Xray Comparison Report</h1>
  <div class="meta-box">
    <div class="meta-row"><strong>Base Snapshot:</strong> {{ baseId }}</div>
    <div class="meta-row"><strong>Target Snapshot:</strong> {{ targetId }}</div>
    <div class="meta-row"><strong>Analysis Fingerprint:</strong> <code>{{ analysisFingerprint }}</code></div>
    <div class="meta-row"><strong>Metric Schema:</strong> {{ metricSchemaVersion }}</div>
    <div class="meta-row"><strong>Coverage Completeness:</strong> {{ completeness }} (skipped elements: {{ skippedElements }})</div>
    <div class="meta-row"><strong>Summary:</strong> {{ summary }}</div>
  </div>
  <table>
    <thead>
      <tr>
        <th>Kind</th>
        <th>Entity</th>
        <th>Path</th>
        <th>Metric Delta</th>
        <th>Findings</th>
        <th>Confidence</th>
        <th>Explanation</th>
      </tr>
    </thead>
    <tbody>
## for row in rows
      <tr>
        <td><span class="badge badge-{{ row.kind }}">{{ row.kind }}</span></td>
        <td><code>{{ row.entityKey }}</code></td>
        <td>{{ row.path }}</td>
        <td>{{ row.metricsSummary }}</td>
        <td>{{ row.findingsSummary }}</td>
        <td>{{ row.confidence }}</td>
        <td>{{ row.explanation }}</td>
      </tr>
## endfor
    </tbody>
  </table>
</body>
</html>
)";

    std::string htmlContent;
    try {
        htmlContent = inja::render(tmpl, data);
    } catch (const std::exception&) {
        return false;
    }

    if (ctx.isCancelled()) return false;
    ctx.report("Writing HTML report to destination", 85, 100);

    return writeReportSafely(options.destination, htmlContent, options.overwriteConfirmed, nullptr, &ctx);
}

// ─────────────────────────────────────────────────────────────
// D23  JsonExporter::write (Серіалізація через nlohmann/json)
// ─────────────────────────────────────────────────────────────
bool JsonExporter::write(const ComparisonReport& report, const ExportOptions& options, const JobContext& ctx)
{
    if (ctx.isCancelled()) return false;
    ctx.report("Serializing report to JSON", 10, 100);

    nlohmann::json root;
    root["schemaVersion"] = "comparison-v1";
    root["baseId"] = report.baseId;
    root["targetId"] = report.targetId;
    root["analysisFingerprint"] = report.analysisFingerprint;
    root["metricSchemaVersion"] = report.metricSchemaVersion;

    // Coverage
    nlohmann::json cov;
    cov["completeness"] = (report.coverage.completeness == Completeness::complete) ? "complete" : "partial";
    cov["skippedElements"] = report.coverage.skippedElements;
    nlohmann::json diags = nlohmann::json::array();
    for (const auto& d : report.coverage.diagnostics) {
        nlohmann::json diag;
        diag["code"] = static_cast<int>(d.code);
        diag["message"] = d.message;
        diag["path"] = d.path.value_or("");
        diag["commit"] = d.commit.value_or("");
        diag["rule"] = d.rule.value_or("");
        diags.push_back(diag);
    }
    cov["diagnostics"] = diags;
    root["coverage"] = cov;

    root["summary"] = summarizeComparison(report.items);

    if (ctx.isCancelled()) return false;
    ctx.report("Serializing comparison items to JSON", 40, 100);

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

    nlohmann::json items = nlohmann::json::array();
    for (std::size_t itemIdx = 0; itemIdx < report.items.size(); ++itemIdx) {
        if ((itemIdx % 50 == 0) && ctx.isCancelled()) {
            return false;
        }
        const auto& item = report.items[itemIdx];
        nlohmann::json it;
        it["id"] = item.id;
        it["kind"] = kindToString(item.kind);
        it["confidence"] = confidenceToString(item.confidence);
        it["entityKey"] = item.entityKey;
        it["explanation"] = item.explanation;

        // SourceRefs
        nlohmann::json refs = nlohmann::json::array();
        for (const auto& r : item.sourceRefs) {
            nlohmann::json ref;
            ref["sourceId"] = r.sourceId;
            ref["relativePath"] = r.relativePath;
            ref["entityId"] = r.entityId;
            ref["range"] = {
                {"startByte", r.range.startByte},
                {"endByte", r.range.endByte},
                {"startLine", r.range.startLine},
                {"endLine", r.range.endLine}
            };
            refs.push_back(ref);
        }
        it["sourceRefs"] = refs;

        // Metrics
        nlohmann::json metrics = nlohmann::json::array();
        for (const auto& m : item.metrics) {
            nlohmann::json mj;
            mj["metricId"] = static_cast<int>(m.metricId);
            mj["metricName"] = metricName(m.metricId);
            if (m.before.has_value()) mj["before"] = *m.before;
            else mj["before"] = nullptr;
            if (m.after.has_value()) mj["after"] = *m.after;
            else mj["after"] = nullptr;
            if (m.delta.has_value()) mj["delta"] = *m.delta;
            else mj["delta"] = nullptr;
            metrics.push_back(mj);
        }
        it["metrics"] = metrics;

        // Findings
        nlohmann::json findings = nlohmann::json::array();
        for (const auto& f : item.findings) {
            nlohmann::json fj;
            fj["ruleId"] = static_cast<int>(f.ruleId);
            fj["ruleName"] = ruleName(f.ruleId);
            fj["state"] = static_cast<int>(f.state);
            findings.push_back(fj);
        }
        it["findings"] = findings;

        items.push_back(it);
    }
    root["items"] = items;

    if (ctx.isCancelled()) return false;
    ctx.report("Dumping formatted JSON string", 75, 100);

    std::string jsonStr = root.dump(2);

    if (ctx.isCancelled()) return false;
    ctx.report("Writing JSON report to destination", 90, 100);

    return writeReportSafely(options.destination, jsonStr, options.overwriteConfirmed, nullptr, &ctx);
}

// ─────────────────────────────────────────────────────────────
// exportReport — головний фасад експорту
// ─────────────────────────────────────────────────────────────
Result<ExportReceipt> exportReport(const ComparisonReport& report,
                                   const ExportOptions& options,
                                   const JobContext& ctx)
{
    if (options.destination.empty()) {
        return Error{ErrorCode::invalid_input, "exportReport: destination path cannot be empty."};
    }

    if (ctx.isCancelled()) return Cancelled{};

    ctx.report("Exporting report", 0, 100);

    if (ctx.isCancelled()) return Cancelled{};

    std::unique_ptr<ReportExporter> exporter;
    if (options.format == ExportFormat::html) {
        exporter = std::make_unique<HtmlExporter>();
    } else {
        exporter = std::make_unique<JsonExporter>();
    }

    if (ctx.isCancelled()) return Cancelled{};

    bool success = exporter->write(report, options, ctx);

    if (ctx.isCancelled()) {
        return Cancelled{};
    }

    if (!success) {
        return Error{
            ErrorCode::export_error,
            "exportReport: failed to write report to " + options.destination.string() +
            " (check permissions or overwriteConfirmed flag)"
        };
    }

    if (ctx.isCancelled()) return Cancelled{};

    ctx.report("Done", 100, 100);

    std::error_code ec;
    auto size = std::filesystem::file_size(options.destination, ec);
    return ExportReceipt{options.destination, ec ? 0 : size};
}

} // namespace xray::review
