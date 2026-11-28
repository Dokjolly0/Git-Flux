#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace gf::terminal::bl
{
    struct CommandToken
    {
        std::string value;       // Unquoted text
        std::size_t startOffset = 0;
    };

    struct ParsedCommandSegment
    {
        std::vector<CommandToken> tokens;
        /// Index of the token currently under/adjacent to the cursor.
        /// Equals `tokens.size()` when the cursor is after trailing whitespace (new empty token).
        std::size_t activeTokenIndex = 0;
        std::size_t activeReplaceStart = 0;
        std::size_t activeReplaceLength = 0;
        std::string activePrefix;
        bool endsWithSpace = false;
    };

    class CommandLineTokenizer
    {
    public:
        /// Tokenizes a full command line up to `cursorPos` and returns the active command segment
        /// (after the last unquoted `&&`, `||`, `;`, or `|` before `cursorPos`).
        [[nodiscard]] static ParsedCommandSegment parseActiveSegment(std::string_view input,
                                                                     std::size_t cursorPos);

        /// Splits a command line into shell-style arguments (unquoted). Returns false if
        /// the line contains unquoted shell metacharacters (`|`, `&`, `;`, `>`, `<`, `$`, `` ` ``, `*`, `?`).
        [[nodiscard]] static bool trySplitSimpleArgs(std::string_view input,
                                                     std::vector<std::string>& outArgs);

        /// Returns true if the command line contains unquoted shell control/chaining
        /// operators (`&&`, `||`, `;`, `|`, `&`, `>`, `<`, or newline).
        [[nodiscard]] static bool containsShellOperators(std::string_view input) noexcept;
    };
} // namespace gf::terminal::bl
