#pragma once

#include "history/types.hpp"
#include <chrono>
#include <vector>

namespace xray::ui::history {

inline std::vector<xray::history::CommitRecord> makeDemoCommits() {
    using namespace std::chrono;
    auto t1 = sys_seconds{ seconds(1700001000) };
    auto t2 = sys_seconds{ seconds(1700002000) };
    auto t3 = sys_seconds{ seconds(1700003000) };
    auto t4 = sys_seconds{ seconds(1700004000) };
    auto t5 = sys_seconds{ seconds(1700005000) };
    auto t6 = sys_seconds{ seconds(1700006000) };

    return {
        { "c6f81a2", { "b5e90d1" }, "Ivan Petrenko", "ivan@example.com", t6, t6, "feat(ui): apply Gruvbox navigation and layout" },
        { "b5e90d1", { "a4d80c3", "f3c70b2" }, "Oleksandr Kostiuk", "kostiuk@example.com", t5, t5, "Merge branch 'feature/analysis' into main" },
        { "a4d80c3", { "e2b60a1" }, "Oleksandr Kostiuk", "kostiuk@example.com", t4, t4, "feat(history): add HistoryQuery and RevisionSelection" },
        { "f3c70b2", { "e2b60a1" }, "Mykola Triguba", "mykola@example.com", t3, t3, "feat(analysis): implement code complexity metrics" },
        { "e2b60a1", { "d1a5090" }, "Oksana Koval", "oksana@example.com", t2, t2, "feat(code): AST entity extraction with Tree-sitter" },
        { "d1a5090", {},             "Ivan Petrenko", "ivan@example.com", t1, t1, "Initial repository structure and contracts" }
    };
}

inline std::vector<xray::history::ChangeRecord> makeDemoChanges() {
    return {
        { "c6f81a2", "b5e90d1", "src/ui/main.cpp", "src/ui/main.cpp", xray::history::ChangeStatus::modified, 45, 12, false },
        { "c6f81a2", "b5e90d1", "assets/logo.png", "assets/logo.png", xray::history::ChangeStatus::modified, std::nullopt, std::nullopt, true },
        { "b5e90d1", "a4d80c3", "src/app/coordinator.cpp", "src/app/coordinator.cpp", xray::history::ChangeStatus::modified, 110, 35, false },
        { "a4d80c3", "e2b60a1", "src/history/types.hpp", "src/history/types.hpp", xray::history::ChangeStatus::added, 180, 0, false },
        { "a4d80c3", "e2b60a1", "tests/history_test.cpp", "tests/history_test.cpp", xray::history::ChangeStatus::added, 140, 0, false },
        { "f3c70b2", "e2b60a1", "src/analysis/engine.cpp", "src/analysis/engine.cpp", xray::history::ChangeStatus::added, 95, 4, false },
        { "e2b60a1", "d1a5090", "legacy/old_parser.cpp", std::nullopt, xray::history::ChangeStatus::deleted, 0, 85, false },
        { "e2b60a1", "d1a5090", "docs/spec_draft.md", "docs/spec_v1.md", xray::history::ChangeStatus::renamed, 20, 5, false }
    };
}

} // namespace xray::ui::history
