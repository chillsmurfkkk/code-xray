#pragma once

#include "code/types.hpp"

#include <memory>

namespace xray::ui {

std::shared_ptr<const xray::code::ParsedSnapshot> makeDemoSnapshot();

} // namespace xray::ui
