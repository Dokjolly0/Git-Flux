#pragma once

#include "core/shared/strong_id.hpp"

namespace gf::repositories::bo
{
    struct RepositoryIdTag
    {
    };

    using RepositoryId = core::StrongId<RepositoryIdTag>;
} // namespace gf::repositories::bo
