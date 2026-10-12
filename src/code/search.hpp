#pragma once

#include "code/types.hpp"
#include "code/walk.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace xray::code {

struct SearchQuery {
    std::string text;
    std::optional<EntityKind> kind;
};

struct SearchMatch {
    EntityId entityId;
    bool exact = false;
};

class SearchVisitor {
public:
    explicit SearchVisitor(SearchQuery query)
        : query_(std::move(query)) {}

    WalkAction operator()(
        const CodeEntity& entity,
        const CodeEntity*
    ) {
        if (query_.kind && entity.kind != *query_.kind) {
            return WalkAction::descend;
        }

        if (!query_.text.empty() && entity.name.find(query_.text) == std::string::npos) {
            return WalkAction::descend;
        }

        matches_.push_back(SearchMatch{
            entity.id,
            !query_.text.empty() && entity.name == query_.text
        });

        return WalkAction::descend;
    }

    const std::vector<SearchMatch>& matches() const {
        return matches_;
    }

private:
    SearchQuery query_;
    std::vector<SearchMatch> matches_;
};

inline std::vector<SearchMatch> searchEntities(
    const CodeEntity& root,
    SearchQuery query
)
{
    SearchVisitor visitor(std::move(query));
    walkEntities(root, visitor);

    std::vector<SearchMatch> matches = visitor.matches();

    std::stable_partition(
        matches.begin(),
        matches.end(),
        [](const SearchMatch& match) {
            return match.exact;
        }
    );

    return matches;
}

} // namespace xray::code
