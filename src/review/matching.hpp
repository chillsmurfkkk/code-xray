#pragma once

#include <vector>
#include <cstddef>

namespace xray::review {

    // Шаблонна функція зіставлення двох колекцій за ключем (Статичний поліморфізм)
    template <typename TLeft, typename TRight, typename KeyFn>
    std::size_t joinByKey(const std::vector<TLeft>& left,
                          const std::vector<TRight>& right,
                          KeyFn keyFn)
    {
        std::size_t matchedCount = 0;

        for (const auto& l : left) {
            for (const auto& r : right) {
                if (keyFn(l) == keyFn(r)) {
                    matchedCount++;
                    break; // Пару знайдено, переходимо до наступного елемента left
                }
            }
        }

        return matchedCount;
    }

} // namespace xray::review