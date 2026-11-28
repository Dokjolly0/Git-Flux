#include "core/modules/terminal/bl/command_line_tokenizer.hpp"

#include <algorithm>

namespace gf::terminal::bl
{
    namespace
    {
        [[nodiscard]] bool isSpace(const char ch) noexcept
        {
            return ch == ' ' || ch == '\t';
        }

        [[nodiscard]] bool isShellOperatorStart(const std::string_view s, const std::size_t pos,
                                                std::size_t& opLen) noexcept
        {
            if (pos >= s.size()) return false;
            const char c = s[pos];
            if (c == ';')
            {
                opLen = 1;
                return true;
            }
            if (c == '|')
            {
                opLen = (pos + 1 < s.size() && s[pos + 1] == '|') ? 2 : 1;
                return true;
            }
            if (c == '&' && pos + 1 < s.size() && s[pos + 1] == '&')
            {
                opLen = 2;
                return true;
            }
            return false;
        }
        [[nodiscard]] bool isShellMetachar(const char c) noexcept
        {
            return c == '|' || c == '&' || c == ';' || c == '>' || c == '<' ||
                c == '$' || c == '`' || c == '*' || c == '?' || c == '(' || c == ')';
        }

        enum class TokenizeMode
        {
            CompletionPrefix,
            SimpleCommandArgs
        };

        // Consumes a single token starting at `i`, returns false if rejected in SimpleCommandArgs mode
        [[nodiscard]] bool readToken(const std::string_view src,
                                     std::size_t& i,
                                     const TokenizeMode mode,
                                     std::string& outValue)
        {
            char quote = '\0';
            while (i < src.size())
            {
                const char c = src[i];
                if (quote != '\0')
                {
                    const bool escQuote = (c == '\\' && quote == '"' && i + 1 < src.size()) &&
                        (mode == TokenizeMode::CompletionPrefix || src[i + 1] == '"' || src[i + 1] == '\\');
                    if (escQuote)
                    {
                        outValue.push_back(src[i + 1]);
                        i += 2;
                        continue;
                    }
                    if (c == quote)
                    {
                        quote = '\0';
                        ++i;
                        continue;
                    }
                    outValue.push_back(c);
                    ++i;
                    continue;
                }

                if (isSpace(c))
                {
                    break;
                }
                if (c == '\'' || c == '"')
                {
                    quote = c;
                    ++i;
                    continue;
                }
                if (mode == TokenizeMode::CompletionPrefix)
                {
                    if (c == '\\' && i + 1 < src.size() &&
                        (src[i + 1] == ' ' || src[i + 1] == '\t' || src[i + 1] == '"' || src[i + 1] == '\''))
                    {
                        outValue.push_back(src[i + 1]);
                        i += 2;
                        continue;
                    }
                }
                else if (isShellMetachar(c))
                {
                    return false;
                }
                outValue.push_back(c);
                ++i;
            }
            return mode == TokenizeMode::CompletionPrefix || quote == '\0';
        }
    } // namespace

    ParsedCommandSegment CommandLineTokenizer::parseActiveSegment(const std::string_view input,
                                                                  std::size_t cursorPos)
    {
        cursorPos = std::min(cursorPos, input.size());
        const std::string_view prefix = input.substr(0, cursorPos);

        // 1. Find the start of the active command segment (after last unquoted &&, ||, ;, |)
        std::size_t segmentStart = 0;
        {
            char quote = '\0';
            for (std::size_t i = 0; i < prefix.size(); ++i)
            {
                const char c = prefix[i];
                if (quote != '\0')
                {
                    if (c == '\\' && quote == '"' && i + 1 < prefix.size())
                    {
                        ++i;
                    }
                    else if (c == quote)
                    {
                        quote = '\0';
                    }
                    continue;
                }
                if (c == '\'' || c == '"')
                {
                    quote = c;
                    continue;
                }
                if (c == '\\' && i + 1 < prefix.size())
                {
                    ++i;
                    continue;
                }
                std::size_t opLen = 0;
                if (isShellOperatorStart(prefix, i, opLen))
                {
                    segmentStart = i + opLen;
                    i += opLen - 1;
                }
            }
        }

        ParsedCommandSegment seg;

        // 2. Tokenize prefix[segmentStart .. cursorPos)
        std::size_t i = segmentStart;
        while (i < prefix.size())
        {
            while (i < prefix.size() && isSpace(prefix[i]))
            {
                ++i;
            }
            if (i >= prefix.size())
            {
                break;
            }

            CommandToken tok;
            tok.startOffset = i;
            (void)readToken(prefix, i, TokenizeMode::CompletionPrefix, tok.value);

            seg.tokens.push_back(std::move(tok));
        }

        const bool trailingSpace = (cursorPos > segmentStart && isSpace(prefix.back()));
        seg.endsWithSpace = trailingSpace;

        if (seg.tokens.empty() || trailingSpace)
        {
            seg.activeTokenIndex = seg.tokens.size();
            seg.activeReplaceStart = cursorPos;
            seg.activeReplaceLength = 0;
            seg.activePrefix.clear();
        }
        else
        {
            seg.activeTokenIndex = seg.tokens.size() - 1;
            const auto& lastTok = seg.tokens.back();
            seg.activeReplaceStart = lastTok.startOffset;
            seg.activeReplaceLength = cursorPos - lastTok.startOffset;
            seg.activePrefix = lastTok.value;
        }

        return seg;
    }

    bool CommandLineTokenizer::trySplitSimpleArgs(const std::string_view input,
                                                  std::vector<std::string>& outArgs)
    {
        outArgs.clear();
        std::size_t i = 0;
        while (i < input.size())
        {
            while (i < input.size() && isSpace(input[i]))
            {
                ++i;
            }
            if (i >= input.size())
            {
                break;
            }

            std::string arg;
            if (!readToken(input, i, TokenizeMode::SimpleCommandArgs, arg))
            {
                return false;
            }
            outArgs.push_back(std::move(arg));
        }
        return !outArgs.empty();
    }

    bool CommandLineTokenizer::containsShellOperators(const std::string_view input) noexcept
    {
        char quote = '\0';
        for (const char c : input)
        {
            if (quote != '\0')
            {
                if (c == quote)
                {
                    quote = '\0';
                }
                continue;
            }
            if (c == '\'' || c == '"')
            {
                quote = c;
                continue;
            }
            if (c == ';' || c == '|' || c == '&' || c == '>' || c == '<' || c == '\n' || c == '\r')
            {
                return true;
            }
        }
        return false;
    }
} // namespace gf::terminal::bl
