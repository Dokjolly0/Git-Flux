#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace gf::git::bo {

struct GitCommitLogItem {
    std::string sha{};
    std::string shortSha{};
    std::vector<std::string> parents{};
    std::string authorName{};
    std::string authorEmail{};
    std::string authorDateIso{};
    std::string committerName{};
    std::string committerEmail{};
    std::string committerDateIso{};
    std::string subject{};
    std::string body{};
    std::vector<std::string> refDecorations{};
};

struct CommitLogOptions {
    std::size_t maxCount{100};
    std::size_t skip{0};
    std::optional<std::string> revisionRange{std::nullopt};
    std::optional<std::string> pathFilter{std::nullopt};
    bool followRenames{false};
    bool topoOrder{false};
    bool dateOrder{false};
};

enum class TagType {
    Lightweight,
    Annotated
};

struct GitTag {
    std::string name{};
    std::string targetSha{};
    std::string taggerName{};
    std::string taggerEmail{};
    std::string tagDateIso{};
    std::string message{};
    TagType type{TagType::Lightweight};
};

struct CreateTagOptions {
    std::string name{};
    std::optional<std::string> targetCommit{std::nullopt};
    std::optional<std::string> message{std::nullopt};
    bool force{false};
};

} // namespace gf::git::bo
