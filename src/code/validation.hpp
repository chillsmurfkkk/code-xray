#pragma once

#include "code/types.hpp"
#include "code/walk.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace xray::code {

struct RangeIssue {
    EntityId entityId;
    std::string message;
};

class RangeValidationVisitor {
public:
explicit RangeValidationVisitor(std::uint64_t sourceSize)
    : sourceSize_(sourceSize) {}

    WalkAction operator()(
        const CodeEntity& entity,
        const CodeEntity* parent
    )
    {
        const auto& range = entity.range;
        const bool bytesOrdered = range.startByte <= range.endByte;

        const bool linesOrdered = range.startLine >= 1 &&
            range.startLine <= range.endLine;

        if (!bytesOrdered) {
            issues_.push_back({
                entity.id,
                "Byte range is reversed"
            });
        }

        if (range.endByte > sourceSize_) {
            issues_.push_back({
                entity.id,
                "Byte range exceeds source size"
            });
        }

        if (!linesOrdered) {
            issues_.push_back({
                entity.id,
                "Line range is invalid"
            });
        }

        if (parent) {
            const auto& outer = parent->range;

            if (bytesOrdered &&
                outer.startByte <= outer.endByte &&
                (range.startByte < outer.startByte ||
                 range.endByte > outer.endByte)) {
                issues_.push_back({
                    entity.id,
                    "Byte range is outside parent"
                });
            }

            if (linesOrdered &&
                outer.startLine >= 1 &&
                outer.startLine <= outer.endLine &&
                (range.startLine < outer.startLine ||
                 range.endLine > outer.endLine)) {
                issues_.push_back({
                    entity.id,
                    "Line range is outside parent"
                });
            }
        }

        return WalkAction::descend;
    }

    const std::vector<RangeIssue>& issues() const {
        return issues_;
    }

private:
    std::uint64_t sourceSize_;
    std::vector<RangeIssue> issues_;
};

inline std::vector<RangeIssue> validateEntityRanges(
    const CodeEntity& root,
    std::uint64_t sourceSize
)
{
    RangeValidationVisitor visitor(sourceSize);
    walkEntities(root, visitor);
    return visitor.issues();
}

} // namespace xray::code
