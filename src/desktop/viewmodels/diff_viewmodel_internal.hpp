#pragma once

#include "desktop/viewmodels/diff_viewmodel.hpp"

#include <QString>
#include <QVariantList>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gf::desktop
{
    /// Everything the view shows for a diff, built off the GUI thread: syntax highlighting,
    /// split/hunk rows, inline-model rows and, for Markdown, the rendered document.
    struct DiffViewModel::PreparedDiff
    {
        bool prepared = false;
        QString languageName;
        QString languageIcon;
        bool isMarkdown = false;
        int maxLineLength = 0;
        QVariantList hunks;
        QVariantList splitLines;
        QVariantList splitOverviewMarkers;
        QVariantList inlineOverviewMarkers;
        QVariantList hunkOverviewMarkers;
        std::vector<gf::git::bo::DiffLine> modelLines;
        std::vector<QString> modelRich;

        bool hasMarkdownDocument = false;
        QVariantList markdownBlocks;
        QVariantList markdownToc;
        QString markdownSource;
        QString markdownNotice;

        std::optional<DiffFileInspection> inspection;
    };

    struct DiffViewModel::DiffFetch
    {
        bool skipped = false;
        std::optional<std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError>> diff;
        std::optional<std::expected<std::string, gf::git::ports::GitOpsError>> markdownContent;
        std::string deletedNotice;
        std::optional<gf::git::bo::FileBlob> oldBlob;
        std::optional<gf::git::bo::FileBlob> newBlob;
        std::string inspectPath;
        bool inspectedRichFile = false;
        std::shared_ptr<const gf::git::bo::FileDiff> comparedWith;
        bool unchanged = false;
        std::optional<PreparedDiff> prepared;
    };

    /// Blame read and turned into view rows on the worker (highlighting included).
    struct DiffViewModel::BlameFetch
    {
        bool skipped = false;
        bool ok = false;
        QVariantList lines;
        int maxLineLength = 0;
        QString languageName;
        QString languageIcon;
    };
} // namespace gf::desktop
