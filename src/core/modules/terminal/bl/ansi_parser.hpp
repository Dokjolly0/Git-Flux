#pragma once

#include "core/modules/terminal/bo/terminal_models.hpp"

#include <string>
#include <string_view>

namespace gf::terminal::bl
{
    class AnsiParser
    {
    public:
        /// Parses a single line of terminal output containing ANSI SGR sequences into styled spans,
        /// plain text, and Qt-compatible rich HTML. Updates `carryStyle` for multi-line sequences.
        [[nodiscard]] static bo::TerminalRenderedLine parseLine(std::string_view rawLine,
                                                                bo::TerminalTextStyle& carryStyle);

        /// Convenience overload starting from default text style.
        [[nodiscard]] static bo::TerminalRenderedLine parseLine(std::string_view rawLine);

        /// Resolves carriage returns (`\r`) within a single line (e.g. git progress updates).
        [[nodiscard]] static std::string resolveCarriageReturns(std::string_view rawLine);

        /// Converts an xterm 256-color index (0..255) to a "#RRGGBB" hex string.
        [[nodiscard]] static std::string xterm256ToHex(int index);

        /// Escapes plain text to safe HTML preserving indentation and consecutive spaces.
        [[nodiscard]] static std::string escapeHtmlPreservingSpaces(std::string_view text);
    };
} // namespace gf::terminal::bl
