#pragma once

#include "core/modules/git/bo/git_command.hpp"

#include <expected>

namespace gf::infra::git {

class GitDiscovery {
public:
    /// Discovers the Git binary on the system and detects (on Windows) whether the path in PATH
    /// points to the `cmd\git.exe` shim, resolving the direct native binary
    /// (`mingw64\bin\git.exe` or equivalent) to eliminate double process spawning overhead.
    [[nodiscard]] static std::expected<gf::git::bo::GitExecutableInfo, gf::git::bo::GitExecutionError>
    discover();
};

} // namespace gf::infra::git
