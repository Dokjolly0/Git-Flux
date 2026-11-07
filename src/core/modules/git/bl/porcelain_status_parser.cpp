#include "core/modules/git/bl/porcelain_status_parser.hpp"

#include <charconv>
#include <optional>

namespace gf::git::bl {

namespace {

[[nodiscard]] std::string_view skipSpacesTokens(std::string_view line, int spaceCount) noexcept {
    std::size_t pos = 0;
    for (int i = 0; i < spaceCount; ++i) {
        pos = line.find(' ', pos);
        if (pos == std::string_view::npos) {
            return {};
        }
        ++pos;
    }
    return line.substr(pos);
}

void parseBranchHeader(std::string_view headerLine, bo::GitBranchHead& branch) noexcept {
    // Format: "# branch.<key> <value>"
    constexpr std::string_view prefix = "# branch.";
    if (!headerLine.starts_with(prefix)) {
        return;
    }
    const std::string_view rest = headerLine.substr(prefix.size());
    if (rest.starts_with("oid ")) {
        branch.oid = std::string(rest.substr(4));
        if (branch.oid == "(initial)") {
            branch.unborn = true;
        }
    } else if (rest.starts_with("head ")) {
        branch.headName = std::string(rest.substr(5));
        if (branch.headName == "(detached)") {
            branch.detached = true;
        }
    } else if (rest.starts_with("upstream ")) {
        branch.upstream = std::string(rest.substr(9));
    } else if (rest.starts_with("ab ")) {
        // Esempio: "+3 -1"
        const std::string_view ab = rest.substr(3);
        const std::size_t spacePos = ab.find(' ');
        if (spacePos != std::string_view::npos) {
            std::string_view aheadToken = ab.substr(0, spacePos);
            std::string_view behindToken = ab.substr(spacePos + 1);
            if (aheadToken.starts_with('+')) {
                aheadToken.remove_prefix(1);
            }
            if (behindToken.starts_with('-')) {
                behindToken.remove_prefix(1);
            }
            std::from_chars(aheadToken.data(), aheadToken.data() + aheadToken.size(), branch.ahead);
            std::from_chars(behindToken.data(), behindToken.data() + behindToken.size(), branch.behind);
        }
    }
}

} // namespace

std::expected<bo::GitRepositoryStatus, std::string>
PorcelainStatusParser::parse(std::string_view rawOutput) {
    bo::GitRepositoryStatus status;
    std::size_t cursor = 0;

    auto nextRecord = [&]() -> std::optional<std::string_view> {
        if (cursor >= rawOutput.size()) {
            return std::nullopt;
        }
        const std::size_t nulPos = rawOutput.find('\0', cursor);
        if (nulPos == std::string_view::npos) {
            std::string_view tail = rawOutput.substr(cursor);
            cursor = rawOutput.size();
            if (!tail.empty() && tail.back() == '\n') {
                tail.remove_suffix(1);
            }
            return tail.empty() ? std::nullopt : std::optional<std::string_view>{tail};
        }
        std::string_view token = rawOutput.substr(cursor, nulPos - cursor);
        cursor = nulPos + 1;
        return token;
    };

    while (auto recordOpt = nextRecord()) {
        const std::string_view rec = *recordOpt;
        if (rec.empty()) {
            continue;
        }

        const char tag = rec[0];
        if (tag == '#') {
            parseBranchHeader(rec, status.branch);
            continue;
        }

        if (tag == '1') {
            // 1 <XY> <sub> <mH> <mI> <mW> <hH> <hI> <path>
            if (rec.size() < 5) {
                return std::unexpected("Record porcelain '1' malformato");
            }
            const std::string_view path = skipSpacesTokens(rec, 8);
            if (path.empty()) {
                return std::unexpected("Path mancante in record porcelain '1'");
            }
            bo::GitFileStatusEntry entry;
            entry.stagedStatus = rec[2];
            entry.unstagedStatus = rec[3];
            entry.path = std::string(path);
            status.entries.push_back(std::move(entry));
        } else if (tag == '2') {
            // 2 <XY> <sub> <mH> <mI> <mW> <hH> <hI> <X><score> <path>\0<origPath>\0
            if (rec.size() < 5) {
                return std::unexpected("Record porcelain '2' malformato");
            }
            const std::string_view path = skipSpacesTokens(rec, 9);
            const auto origPathOpt = nextRecord();
            if (path.empty() || !origPathOpt) {
                return std::unexpected("Path o origPath mancante in record porcelain '2'");
            }
            bo::GitFileStatusEntry entry;
            entry.stagedStatus = rec[2];
            entry.unstagedStatus = rec[3];
            entry.path = std::string(path);
            entry.originalPath = std::string(*origPathOpt);
            status.entries.push_back(std::move(entry));
        } else if (tag == 'u') {
            // u <XY> <sub> <m1> <m2> <m3> <mW> <h1> <h2> <h3> <path>
            if (rec.size() < 5) {
                return std::unexpected("Record porcelain 'u' malformato");
            }
            const std::string_view path = skipSpacesTokens(rec, 10);
            if (path.empty()) {
                return std::unexpected("Path mancante in record porcelain 'u'");
            }
            bo::GitFileStatusEntry entry;
            entry.stagedStatus = rec[2];
            entry.unstagedStatus = rec[3];
            entry.isConflict = true;
            entry.path = std::string(path);
            status.entries.push_back(std::move(entry));
        } else if (tag == '?') {
            // ? <path>
            if (rec.size() < 3) {
                return std::unexpected("Record porcelain '?' malformato");
            }
            bo::GitFileStatusEntry entry;
            entry.stagedStatus = '?';
            entry.unstagedStatus = '?';
            entry.isUntracked = true;
            entry.path = std::string(rec.substr(2));
            status.entries.push_back(std::move(entry));
        }
    }

    return status;
}

} // namespace gf::git::bl
