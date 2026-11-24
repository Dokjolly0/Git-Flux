#pragma once

#include "core/shared/strong_id.hpp"

namespace gf::groups::bo
{
    struct GroupIdTag
    {
    };

    using GroupId = core::StrongId<GroupIdTag>;
} // namespace gf::groups::bo
