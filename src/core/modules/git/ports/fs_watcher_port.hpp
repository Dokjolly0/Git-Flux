#pragma once

#include "core/modules/git/bo/fs_watcher_models.hpp"

#include <expected>
#include <filesystem>
#include <functional>
#include <string>

namespace gf::git::ports
{
    struct FsWatcherError
    {
        std::string message;

        auto operator<=>(const FsWatcherError&) const = default;
    };

    using FsRawEventCallback = std::function<void(const bo::FsRawEvent &)>;

    class IFsWatcherPort
    {
    public:
        virtual ~IFsWatcherPort() = default;

        [[nodiscard]] virtual std::expected<void, FsWatcherError>
        watchRepository(const std::filesystem::path& repoPath) = 0;

        [[nodiscard]] virtual std::expected<void, FsWatcherError>
        unwatchRepository(const std::filesystem::path& repoPath) = 0;

        [[nodiscard]] virtual bool isWatching(const std::filesystem::path& repoPath) const = 0;

        virtual void setEventCallback(FsRawEventCallback callback) = 0;
    };
} // namespace gf::git::ports
