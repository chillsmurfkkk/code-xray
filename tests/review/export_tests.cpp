#include "review/exporter.hpp"
#include "review/api.hpp"
#include "review/types.hpp"
#include "common/job.hpp"
#include "common/result.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stop_token>
#include <string>

#include <nlohmann/json.hpp>

using namespace xray;
using namespace xray::review;

// ─── D21 escapeHtmlText ─────────────────────────────────────

static void test_escapeHtmlText()
{
    assert(escapeHtmlText("hello") == "hello");
    assert(escapeHtmlText("<script>") == "&lt;script&gt;");
    assert(escapeHtmlText("a & b") == "a &amp; b");
    assert(escapeHtmlText("x\"y") == "x&quot;y");
    assert(escapeHtmlText("it's") == "it&#39;s");
    // Уникнення подвійного екранування
    assert(escapeHtmlText("&amp;") == "&amp;");
    assert(escapeHtmlText("&lt;") == "&lt;");
    assert(escapeHtmlText("&gt;") == "&gt;");
    assert(escapeHtmlText("&quot;") == "&quot;");
    assert(escapeHtmlText("&#39;") == "&#39;");
    // Звичайний & все ще екранується
    assert(escapeHtmlText("AT&T") == "AT&amp;T");
    std::cout << "  [OK] D21: escapeHtmlText\n";
}

// ─── D24 writeReportSafely ──────────────────────────────────

static void test_writeReportSafely()
{
    auto tmpDir = std::filesystem::temp_directory_path() / "xray_export_test";
    std::filesystem::create_directories(tmpDir);

    auto dest = tmpDir / "test_report.txt";
    std::filesystem::remove(dest);

    // 1. Записати новий файл
    std::uintmax_t written = 0;
    bool ok = writeReportSafely(dest, "hello world", true, &written);
    assert(ok);
    assert(written == 11);
    assert(std::filesystem::exists(dest));

    // 2. Перезапис без підтвердження — відмова
    bool denied = writeReportSafely(dest, "overwrite", false);
    assert(!denied);

    // 3. Перезапис з підтвердженням — успіх
    bool overwritten = writeReportSafely(dest, "new content", true, &written);
    assert(overwritten);
    assert(written == 11);

    // 4. Порожній шлях — помилка
    assert(!writeReportSafely("", "data", true));

    // Cleanup
    std::filesystem::remove_all(tmpDir);
    std::cout << "  [OK] D24: writeReportSafely\n";
}

// ─── D20 buildReportModel ───────────────────────────────────

static void test_buildReportModel()
{
    ComparisonReport report;
    report.baseId = "base-snap";
    report.targetId = "target-snap";
    report.analysisFingerprint = "fp-123";
    report.metricSchemaVersion = "metrics-v1";

    ReviewItem item;
    item.id = "item-1";
    item.entityKey = "processData()";
    item.kind = ReviewKind::changed;
    item.confidence = Confidence::exact;
    item.explanation = "Lines increased";

    MetricDelta m;
    m.metricId = analysis::MetricId::function_lines;
    m.before = 10;
    m.after = 25;
    m.delta = 15;
    item.metrics.push_back(m);

    SourceRef ref;
    ref.relativePath = "core/data.cpp";
    item.sourceRefs.push_back(ref);

    report.items.push_back(item);

    ReportModel model = buildReportModel(report);
    assert(model.baseId == "base-snap");
    assert(model.targetId == "target-snap");
    assert(model.totalItems == 1);
    assert(model.rows.size() == 1);
    assert(model.rows[0].kind == "changed");
    assert(model.rows[0].entityKey == "processData()");
    assert(model.rows[0].confidence == "exact");
    assert(model.rows[0].path == "core/data.cpp");
    // Перевіряємо що назва метрики та значення до -> після присутні!
    assert(model.rows[0].metricsSummary.find("lines: 10 -> 25 (+15)") != std::string::npos);
    std::cout << "  [OK] D20: buildReportModel (with descriptive metric and rule names)\n";
}

// ─── D22 HtmlExporter::write ────────────────────────────────

static void test_htmlExporter()
{
    auto tmpDir = std::filesystem::temp_directory_path() / "xray_export_test";
    std::filesystem::create_directories(tmpDir);
    auto dest = tmpDir / "report.html";
    std::filesystem::remove(dest);

    ComparisonReport report;
    report.baseId = "b";
    report.targetId = "t";
    report.analysisFingerprint = "fp";
    report.metricSchemaVersion = "v1";
    report.coverage.completeness = Completeness::partial;
    report.coverage.skippedElements = 3;

    ReviewItem item;
    item.id = "i1";
    // Тестуємо уникнення HTML-ін'єкцій (<script>, &, quotes)
    item.entityKey = "fn<T>(&param, \"arg\")";
    item.kind = ReviewKind::added;
    item.confidence = Confidence::exact;
    item.explanation = "<script>alert('xss')</script>";
    report.items.push_back(item);

    HtmlExporter exporter;
    ExportOptions opts;
    opts.destination = dest;
    opts.overwriteConfirmed = true;

    assert(exporter.write(report, opts));
    assert(std::filesystem::exists(dest));

    // Читання вмісту з явним закриттям ifstream для Windows
    std::string content;
    {
        std::ifstream in(dest);
        content.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    assert(content.find("<html") != std::string::npos);
    assert(content.find("Coverage Completeness:") != std::string::npos);
    assert(content.find("partial (skipped elements: 3)") != std::string::npos);

    // Перевірка захисту від HTML-ін'єкцій:
    assert(content.find("<script>") == std::string::npos);
    assert(content.find("&lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;") != std::string::npos);
    assert(content.find("fn&lt;T&gt;(&amp;param, &quot;arg&quot;)") != std::string::npos);

    // Поліморфний виклик
    ReportExporter* base = &exporter;
    assert(base->write(report, opts));

    std::filesystem::remove_all(tmpDir);
    std::cout << "  [OK] D22: HtmlExporter::write (with inja templating, coverage, and HTML injection prevention)\n";
}

// ─── D23 JsonExporter::write ────────────────────────────────

static void test_jsonExporter()
{
    auto tmpDir = std::filesystem::temp_directory_path() / "xray_export_test";
    std::filesystem::create_directories(tmpDir);
    auto dest = tmpDir / "report.json";
    std::filesystem::remove(dest);

    ComparisonReport report;
    report.baseId = "snap-base";
    report.targetId = "snap-target";
    report.analysisFingerprint = "fp-abc";
    report.metricSchemaVersion = "metrics-v1";
    report.coverage.completeness = Completeness::partial;
    report.coverage.skippedElements = 5;

    Error diag;
    diag.code = ErrorCode::parse_incomplete;
    diag.message = "Syntax error in macro";
    report.coverage.diagnostics.push_back(diag);

    ReviewItem item;
    item.id = "i1";
    // Тестуємо спеціальні символи, лапки, переноси рядків для P1!
    item.entityKey = "calc(\"special\\path\", 42)\ninline";
    item.kind = ReviewKind::changed;
    item.confidence = Confidence::exact;
    item.explanation = "Detailed \"quote\" and \\slash explanation";

    SourceRef ref;
    ref.sourceId = "src-1";
    ref.relativePath = "src/math.cpp";
    ref.entityId = "fn-1";
    ref.range = {0, 100, 1, 10};
    item.sourceRefs.push_back(ref);

    MetricDelta m;
    m.metricId = analysis::MetricId::function_lines;
    m.before = 10;
    m.after = 20;
    m.delta = 10;
    item.metrics.push_back(m);

    FindingChange f;
    f.ruleId = analysis::RuleId::long_function;
    f.state = FindingState::new_finding;
    item.findings.push_back(f);

    report.items.push_back(item);

    JsonExporter exporter;
    ExportOptions opts;
    opts.destination = dest;
    opts.overwriteConfirmed = true;

    assert(exporter.write(report, opts));
    assert(std::filesystem::exists(dest));

    // Зворотне читання через nlohmann::json (перевірка валідності JSON)
    std::string jsonStr;
    {
        std::ifstream in(dest);
        jsonStr.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    auto parsed = nlohmann::json::parse(jsonStr);

    assert(parsed["schemaVersion"] == "comparison-v1");
    assert(parsed["coverage"]["completeness"] == "partial");
    assert(parsed["coverage"]["skippedElements"] == 5);
    assert(parsed["coverage"]["diagnostics"].size() == 1);
    assert(parsed["coverage"]["diagnostics"][0]["message"] == "Syntax error in macro");

    assert(parsed["items"].size() == 1);
    const auto& it = parsed["items"][0];
    assert(it["id"] == "i1");
    assert(it["kind"] == "changed");
    assert(it["confidence"] == "exact");
    assert(it["entityKey"] == "calc(\"special\\path\", 42)\ninline");
    assert(it["explanation"] == "Detailed \"quote\" and \\slash explanation");
    assert(it["sourceRefs"].size() == 1);
    assert(it["sourceRefs"][0]["sourceId"] == "src-1");
    assert(it["sourceRefs"][0]["relativePath"] == "src/math.cpp");
    assert(it["metrics"][0]["metricName"] == "lines");
    assert(it["metrics"][0]["delta"] == 10);
    assert(it["findings"][0]["ruleName"] == "long_function");

    // Поліморфний виклик
    ReportExporter* base = &exporter;
    assert(base->write(report, opts));

    std::filesystem::remove_all(tmpDir);
    std::cout << "  [OK] D23: JsonExporter::write (valid nlohmann/json, round-trip parsed, full coverage & sourceRefs)\n";
}

// ─── exportReport facade ────────────────────────────────────

static void test_exportReport_html_and_json()
{
    auto tmpDir = std::filesystem::temp_directory_path() / "xray_export_test";
    std::filesystem::create_directories(tmpDir);

    ComparisonReport report;
    report.baseId = "b";
    report.targetId = "t";
    report.analysisFingerprint = "fp";
    report.metricSchemaVersion = "v1";

    JobContext ctx;

    // HTML
    ExportOptions htmlOpts;
    htmlOpts.destination = tmpDir / "facade.html";
    htmlOpts.format = ExportFormat::html;
    htmlOpts.overwriteConfirmed = true;
    auto resHtml = exportReport(report, htmlOpts, ctx);
    assert(std::holds_alternative<ExportReceipt>(resHtml));
    assert(std::filesystem::exists(htmlOpts.destination));

    // JSON
    ExportOptions jsonOpts;
    jsonOpts.destination = tmpDir / "facade.json";
    jsonOpts.format = ExportFormat::json;
    jsonOpts.overwriteConfirmed = true;
    auto resJson = exportReport(report, jsonOpts, ctx);
    assert(std::holds_alternative<ExportReceipt>(resJson));
    assert(std::filesystem::exists(jsonOpts.destination));

    // Empty destination
    ExportOptions emptyOpts;
    auto resEmpty = exportReport(report, emptyOpts, ctx);
    assert(std::holds_alternative<Error>(resEmpty));

    // Cancellation: скасування у початковому прогресі зберігає старий файл непошкодженим
    auto existingFile = tmpDir / "must_survive.html";
    {
        std::ofstream out(existingFile);
        out << "ORIGINAL UNTOUCHED CONTENT";
    }

    std::stop_source stopSrc;
    JobContext cancelOnProgressCtx{
        1,
        stopSrc.get_token(),
        [&stopSrc](const Progress&) {
            stopSrc.request_stop();
        }
    };

    ExportOptions cancelOpts;
    cancelOpts.destination = existingFile;
    cancelOpts.format = ExportFormat::html;
    cancelOpts.overwriteConfirmed = true;

    auto resCancelled = exportReport(report, cancelOpts, cancelOnProgressCtx);
    assert(std::holds_alternative<Cancelled>(resCancelled));

    // Перевіряємо що старий файл НЕ був перезаписаний:
    std::string existingContent;
    {
        std::ifstream in(existingFile);
        existingContent.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    assert(existingContent == "ORIGINAL UNTOUCHED CONTENT");

    std::filesystem::remove_all(tmpDir);
    std::cout << "  [OK] exportReport: HTML, JSON, empty path, and cancellation during progress preserving old file\n";
}

// ─── Тест скасування під час формування великого звіту ───────

static void test_cancellation_during_large_report_generation()
{
    auto tmpDir = std::filesystem::temp_directory_path() / "xray_cancel_large_test";
    std::filesystem::create_directories(tmpDir);

    // Створюємо великий звіт (200 елементів)
    ComparisonReport report;
    report.baseId = "base-large";
    report.targetId = "target-large";
    report.analysisFingerprint = "fp-large";
    report.metricSchemaVersion = "1.0";
    report.coverage.completeness = Completeness::complete;

    for (int i = 0; i < 200; ++i) {
        ReviewItem item;
        item.id = "item-" + std::to_string(i);
        item.entityKey = "func_" + std::to_string(i) + "()";
        item.kind = (i % 2 == 0) ? ReviewKind::changed : ReviewKind::added;
        item.confidence = Confidence::exact;
        item.explanation = "Generated item " + std::to_string(i);

        MetricDelta m;
        m.metricId = analysis::MetricId::function_lines;
        m.before = 10;
        m.after = 20;
        m.delta = 10;
        item.metrics.push_back(m);

        SourceRef ref;
        ref.relativePath = "src/module/file_" + std::to_string(i) + ".cpp";
        item.sourceRefs.push_back(ref);

        report.items.push_back(item);
    }

    // 1. Скасування під час формування HTML звіту зі збереженням старого файлу
    auto existingHtml = tmpDir / "existing_large_report.html";
    {
        std::ofstream out(existingHtml);
        out << "IMPORTANT PRE-EXISTING HTML CONTENT";
    }

    std::stop_source stopSrcHtml;
    int progressCount = 0;
    JobContext cancelHtmlCtx{
        101,
        stopSrcHtml.get_token(),
        [&stopSrcHtml, &progressCount](const Progress& p) {
            ++progressCount;
            // Скасовуємо під час формування моделі або рендерингу
            if (p.completed >= 30 || progressCount >= 2) {
                stopSrcHtml.request_stop();
            }
        }
    };

    ExportOptions htmlOpts;
    htmlOpts.destination = existingHtml;
    htmlOpts.format = ExportFormat::html;
    htmlOpts.overwriteConfirmed = true;

    auto resHtml = exportReport(report, htmlOpts, cancelHtmlCtx);
    assert(std::holds_alternative<Cancelled>(resHtml));

    // Перевіряємо що старий файл цілий
    std::string htmlContent;
    {
        std::ifstream in(existingHtml);
        htmlContent.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    assert(htmlContent == "IMPORTANT PRE-EXISTING HTML CONTENT");

    // 2. Скасування під час формування JSON звіту зі збереженням старого файлу
    auto existingJson = tmpDir / "existing_large_report.json";
    {
        std::ofstream out(existingJson);
        out << "{\"preserved\": true, \"data\": 12345}";
    }

    std::stop_source stopSrcJson;
    progressCount = 0;
    JobContext cancelJsonCtx{
        102,
        stopSrcJson.get_token(),
        [&stopSrcJson, &progressCount](const Progress& p) {
            ++progressCount;
            if (p.completed >= 40 || progressCount >= 2) {
                stopSrcJson.request_stop();
            }
        }
    };

    ExportOptions jsonOpts;
    jsonOpts.destination = existingJson;
    jsonOpts.format = ExportFormat::json;
    jsonOpts.overwriteConfirmed = true;

    auto resJson = exportReport(report, jsonOpts, cancelJsonCtx);
    assert(std::holds_alternative<Cancelled>(resJson));

    std::string jsonContent;
    {
        std::ifstream in(existingJson);
        jsonContent.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    assert(jsonContent == "{\"preserved\": true, \"data\": 12345}");

    // Перевіряємо що тимчасові файли прибрано
    for (const auto& entry : std::filesystem::directory_iterator(tmpDir)) {
        auto filename = entry.path().filename().string();
        assert(filename.find(".tmp.") == std::string::npos);
        assert(filename.find(".bak.") == std::string::npos);
    }

    std::filesystem::remove_all(tmpDir);
    std::cout << "  [OK] exportReport: cancellation during large report generation preserves existing files\n";
}

// ─── main ───────────────────────────────────────────────────

int main()
{
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    std::cout << "=== export tests ===\n";
    test_escapeHtmlText();
    test_writeReportSafely();
    test_buildReportModel();
    test_htmlExporter();
    test_jsonExporter();
    test_exportReport_html_and_json();
    test_cancellation_during_large_report_generation();

    std::cout << "\nAll PR4 export_tests PASSED 100%!\n";
    return 0;
}
