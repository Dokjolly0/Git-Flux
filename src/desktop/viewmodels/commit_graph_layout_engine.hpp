#pragma once

#include "core/modules/git/bo/history_models.hpp"

#include <QString>
#include <QVariantList>
#include <string>
#include <unordered_map>
#include <vector>

namespace gf::desktop
{
    struct CommitGraphNode
    {
        int lane = 0;
        QString color;
        QVariantList routes;
        bool isOrphan = false;
        bool isStash = false;
        QString stashSelector;
    };

    struct CommitGraphLayout
    {
        std::vector<CommitGraphNode> items;
        int maxLanes = 1;
    };

    struct InferredBranchInfo
    {
        QString name;
        QString borderColor;
        QString bgColor;
    };

    class CommitGraphLayoutEngine final
    {
    public:
        [[nodiscard]] static CommitGraphLayout buildGraphLayout(
            const std::vector<git::bo::GitCommitLogItem>& commits);

        [[nodiscard]] static std::unordered_map<std::string, InferredBranchInfo> computeInferredBranches(
            const std::vector<git::bo::GitCommitLogItem>& rawCommits);

        [[nodiscard]] static QVariantList computeStructuredDecorations(
            const std::vector<std::string>& rawDecorations);

        [[nodiscard]] static bool isStashDecoration(const std::string& dec);
        [[nodiscard]] static bool isStashCommit(const git::bo::GitCommitLogItem& commit);
        [[nodiscard]] static QString extractStashSelector(const std::vector<std::string>& decorations);
    };
} // namespace gf::desktop
