#include "core/modules/git/bl/commit_log_parser.hpp"

#include <cctype>

namespace gf::git::bl
{
    namespace
    {
        [[nodiscard]] std::string_view trim(std::string_view s) noexcept
        {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
            {
                s.remove_prefix(1);
            }
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
            {
                s.remove_suffix(1);
            }
            return s;
        }

        [[nodiscard]] std::vector<std::string> splitSpaces(std::string_view s)
        {
            std::vector<std::string> tokens;
            s = trim(s);
            while (!s.empty())
            {
                auto nextSpace = s.find(' ');
                if (nextSpace == std::string_view::npos)
                {
                    tokens.emplace_back(s);
                    break;
                }
                auto token = s.substr(0, nextSpace);
                if (!token.empty())
                {
                    tokens.emplace_back(token);
                }
                s = trim(s.substr(nextSpace + 1));
            }
            return tokens;
        }

        [[nodiscard]] std::vector<std::string> splitDecorations(std::string_view s)
        {
            std::vector<std::string> decorations;
            s = trim(s);
            while (!s.empty())
            {
                auto nextComma = s.find(',');
                if (nextComma == std::string_view::npos)
                {
                    auto dec = trim(s);
                    if (!dec.empty())
                    {
                        decorations.emplace_back(dec);
                    }
                    break;
                }
                auto dec = trim(s.substr(0, nextComma));
                if (!dec.empty())
                {
                    decorations.emplace_back(dec);
                }
                s = s.substr(nextComma + 1);
            }
            return decorations;
        }
    } // namespace

    std::vector<bo::GitCommitLogItem> CommitLogParser::parse(std::string_view rawOutput)
    {
        std::vector<bo::GitCommitLogItem> commits;
        if (rawOutput.empty())
        {
            return commits;
        }

        constexpr char recordSeparator = '\x1e';
        constexpr char fieldSeparator = '\0';

        std::size_t start = 0;
        while (start < rawOutput.size())
        {
            auto end = rawOutput.find(recordSeparator, start);
            std::string_view record;
            if (end == std::string_view::npos)
            {
                record = rawOutput.substr(start);
                start = rawOutput.size();
            }
            else
            {
                record = rawOutput.substr(start, end - start);
                start = end + 1;
            }

            // If record contains only whitespace or newlines, discard it
            if (trim(record).empty())
            {
                continue;
            }

            // Extract NUL-separated fields
            std::vector<std::string_view> fields;
            fields.reserve(12);
            std::size_t fieldStart = 0;
            while (fieldStart <= record.size())
            {
                auto fieldEnd = record.find(fieldSeparator, fieldStart);
                if (fieldEnd == std::string_view::npos)
                {
                    fields.push_back(record.substr(fieldStart));
                    break;
                }
                fields.push_back(record.substr(fieldStart, fieldEnd - fieldStart));
                fieldStart = fieldEnd + 1;
            }

            if (fields.size() < 11)
            {
                // Malformed record, skip it
                continue;
            }

            bo::GitCommitLogItem item;
            item.sha = std::string(trim(fields[0]));
            item.shortSha = std::string(trim(fields[1]));
            item.parents = splitSpaces(fields[2]);
            item.authorName = std::string(fields[3]);
            item.authorEmail = std::string(fields[4]);
            item.authorDateIso = std::string(trim(fields[5]));
            item.committerName = std::string(fields[6]);
            item.committerEmail = std::string(fields[7]);
            item.committerDateIso = std::string(trim(fields[8]));
            item.refDecorations = splitDecorations(fields[9]);
            item.subject = std::string(fields[10]);

            if (fields.size() >= 12)
            {
                item.body = std::string(trim(fields[11]));
            }

            commits.push_back(std::move(item));
        }

        return commits;
    }
} // namespace gf::git::bl
