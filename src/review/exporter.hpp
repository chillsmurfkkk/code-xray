#pragma once

#include "review/types.hpp"
#include <string>

namespace xray::review {
    
class ReportExporter {
    public:
    virtual ~ReportExporter() = default;

    virtual bool write(const ComparisonReport& report, const ExportOptions& options) = 0;
};

    class HtmlExporter : public ReportExporter {
        public:
        // TODO: stub — реальний експорт у HTML-файл у наступному PR
        bool write(const ComparisonReport& /*report*/, const ExportOptions& options) override {
            return !options.destination.empty();
        }
    };

    class JsonExporter : public ReportExporter {
    public:
        // TODO: stub — реальний експорт у JSON-файл у наступному PR
        bool write(const ComparisonReport& /*report*/, const ExportOptions& options) override {
            return !options.destination.empty();
        }
    };

} // namespace xray::review
