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
    // Заглушка для першого PR: реальний запис HTML файлу буде додано на етапі експорту
    bool write(const ComparisonReport& /*report*/, const ExportOptions& /*options*/) override {
        return false;
    }
};

class JsonExporter : public ReportExporter {
public:
    // Заглушка для першого PR: реальний запис JSON файлу буде додано на етапі експорту
    bool write(const ComparisonReport& /*report*/, const ExportOptions& /*options*/) override {
        return false;
    }
};

} // namespace xray::review
