#pragma once

#include "core/modules/git/bo/diff_models.hpp"
#include "core/modules/syntax/language_detector.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace gf::core::syntax {

enum class MarkdownBlockKind {
    Heading,
    Paragraph,
    CodeBlock,
    Blockquote,
    ListItem,
    Table,
    ThematicBreak,
    BadgeRow,
    DeletedSnippet
};

enum class MarkdownChangeStatus {
    Unchanged,
    Added,
    Modified,
    Deleted
};

enum class MarkdownAlertType {
    None,
    Note,
    Tip,
    Important,
    Warning,
    Caution
};

struct MarkdownBadge {
    std::string label;
    std::string href;
    std::string color;
};

struct MarkdownBlock {
    MarkdownBlockKind kind = MarkdownBlockKind::Paragraph;
    int level = 1;                  // Heading: 1..6
    int indentLevel = 0;            // ListItem nesting depth (0+)
    bool ordered = false;
    bool isTask = false;
    bool taskChecked = false;
    bool centered = false;
    bool isDiagram = false;         // CodeBlock: mermaid, plantuml, etc.
    std::string listMarker;         // "•", "1.", etc.

    MarkdownAlertType alertType = MarkdownAlertType::None;
    std::string rawText;            // Plain title / raw code block text / raw text
    std::string html;               // RichText HTML (or <pre>...</pre> for CodeBlock)
    std::string anchor;             // Heading GitHub slug
    std::string languageTag;        // Fenced code info tag
    Language detectedLanguage = Language::Unknown;

    std::vector<std::string> tableHeaders;
    std::vector<std::string> tableAlignments; // "left", "center", "right"
    std::vector<std::vector<std::string>> tableRows;

    std::vector<MarkdownBadge> badges;

    // 1-based line range in Markdown source + Rich Diff correlation
    int startLine = 0;
    int endLine = 0;
    MarkdownChangeStatus changeStatus = MarkdownChangeStatus::Unchanged;
    int addedLineCount = 0;
    int deletedLineCount = 0;
    std::vector<std::string> deletedLines;
};

struct MarkdownTocEntry {
    int level = 1;
    std::string title;
    std::string anchor;
    int blockIndex = 0;
    int startLine = 0;
};

struct MarkdownDocument {
    std::vector<MarkdownBlock> blocks;
    std::vector<MarkdownTocEntry> toc;
};

class MarkdownParser {
public:
    [[nodiscard]] static MarkdownDocument parse(std::string_view markdownText) noexcept;

    [[nodiscard]] static MarkdownDocument parseWithDiff(
        std::string_view markdownText,
        const gf::git::bo::FileDiff* diff) noexcept;

    [[nodiscard]] static std::string toGithubSlug(std::string_view headingText) noexcept;
    [[nodiscard]] static std::string renderInlineHtml(std::string_view text) noexcept;
};

} // namespace gf::core::syntax
