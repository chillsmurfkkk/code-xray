#pragma once

#include "code/types.hpp"

#include <vector>

namespace xray::code {

    enum class WalkAction {
        descend,
        skip_children,
        stop
    };

    template<class Visitor>
    bool walkEntities(
        const CodeEntity& root,
        Visitor& visitor
    )
    {
        struct Frame {
            const CodeEntity* entity;
            const CodeEntity* parent;
        };

        std::vector<Frame> pending{{&root, nullptr}};

        while (!pending.empty()) {
            const auto current = pending.back();
            pending.pop_back();

            const auto action = visitor(*current.entity, current.parent);

            if (action == WalkAction::stop) {
                return false;
            }

            if (action == WalkAction::skip_children) {
                continue;
            }

            const auto& children = current.entity->children;

            for (auto it = children.rbegin(); it != children.rend(); it++) {
                if (*it) {
                    pending.push_back(Frame{
                        it->get(),
                        current.entity
                    });
                }
            }
        }

        return true;
    }

} // namespace xray::code
