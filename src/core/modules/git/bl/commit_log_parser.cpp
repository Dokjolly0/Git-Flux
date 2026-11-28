#include "core/modules/git/bl/commit_log_parser.hpp"
#include "core/shared/string_utils.hpp"

namespace gf::git::bl
{
    using core::trim;

    namespace
    {
        [[nodiscard]] std::vector<std::string> splitSpaces(std::string_view s)
        {
            std::vector<std::string> tokens;
            s = trim(s);
            while (!s.empty())
            {
                const auto nextSpace = s.find(' ');
                if (nextSpace == std::string_view::npos)
                {
                    tokens.emplace_back(s);
                    break;
                }
                if (const auto token = s.substr(0, nextSpace); !token.empty())
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
                const auto nextComma = s.find(',');
                if (nextComma == std::string_view::npos)
                {
                    if (const auto dec = trim(s); !dec.empty())
                    {
                        decorations.emplace_back(dec);
                    }
                    break;
                }
                if (const auto dec = trim(s.substr(0, nextComma)); !dec.empty())
                {
                    decorations.emplace_back(dec);
                }
                s = s.substr(nextComma + 1);
            }
            return decorations;
        }
    } // namespace

    std::vector<bo::GitCommitLogItem> CommitLogParser::parse(const std::string_view rawOutput)
    {
        std::vector<bo::GitCommitLogItem> commits;
        if (rawOutput.empty())
        {
            return commits;
        }

        std::size_t start = 0;
        while (start < rawOutput.size())
        {
            constexpr char recordSeparator = '\x1e';
            const auto end = rawOutput.find(recordSeparator, start);
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
                constexpr char fieldSeparator = '\0';
                const auto fieldEnd = record.find(fieldSeparator, fieldStart);
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
