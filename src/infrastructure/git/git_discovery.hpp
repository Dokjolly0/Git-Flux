#pragma once

#include "core/modules/git/bo/git_command.hpp"

#include <expected>

namespace gf::infra::git {

class GitDiscovery {
public:
    /// Rileva il binario Git nel sistema e identifica (su Windows) se il path nel PATH
    /// punta allo shim `cmd\git.exe`, risolvendo anche il binario nativo diretto
    /// (`mingw64\bin\git.exe` o equivalente) per eliminare il doppio spawn.
    [[nodiscard]] static std::expected<gf::git::bo::GitExecutableInfo, gf::git::bo::GitExecutionError>
    discover();
};

} // namespace gf::infra::git
