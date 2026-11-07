#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace gf::git::bo {

enum class ConflictSide {
    Base,    // Stage 1: comune antenato
    Ours,    // Stage 2: branch corrente / HEAD
    Theirs,  // Stage 3: branch in arrivo / incoming
    Custom   // Risoluzione combinata o manuale
};

struct ConflictHunk {
    std::size_t hunkIndex{0};
    std::size_t startLine{0};
    std::string baseContent{};
    std::string oursContent{};
    std::string theirsContent{};
    std::optional<ConflictSide> chosenSide{std::nullopt};
    std::optional<std::string> customContent{std::nullopt};
    bool isResolved{false};

    [[nodiscard]] std::string resolvedContent() const {
        if (!isResolved) {
            return {};
        }
        if (customContent.has_value()) {
            return *customContent;
        }
        if (chosenSide.has_value()) {
            switch (*chosenSide) {
            case ConflictSide::Base:   return baseContent;
            case ConflictSide::Ours:   return oursContent;
            case ConflictSide::Theirs: return theirsContent;
            case ConflictSide::Custom: return customContent.value_or("");
            }
        }
        return {};
    }
};

struct ConflictFile {
    std::string filePath{};
    bool isBinary{false};
    std::vector<ConflictHunk> hunks{};
    std::string fullContentWithMarkers{};

    [[nodiscard]] bool allHunksResolved() const noexcept {
        if (hunks.empty()) {
            return true;
        }
        for (const auto& hunk : hunks) {
            if (!hunk.isResolved) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::size_t totalHunks() const noexcept {
        return hunks.size();
    }

    [[nodiscard]] std::size_t resolvedHunksCount() const noexcept {
        std::size_t count = 0;
        for (const auto& hunk : hunks) {
            if (hunk.isResolved) {
                ++count;
            }
        }
        return count;
    }
};

struct ApplyConflictResolutionOptions {
    std::string filePath{};
    std::string resolvedContent{};
    bool autoStage{true};
};

} // namespace gf::git::bo
