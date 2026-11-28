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
    };
} // namespace gf::terminal::bl
