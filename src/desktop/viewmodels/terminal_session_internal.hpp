#pragma once

#include "desktop/viewmodels/terminal_session_viewmodel.hpp"

#include "core/modules/terminal/bo/terminal_models.hpp"

#include <QString>
#include <QStringList>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace gf::desktop
{
    inline constexpr int kTerminalPagerLineThreshold = 28;

    [[nodiscard]] inline std::string_view trimTerminalSv(std::string_view sv) noexcept
    {
        while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '\n'))
        {
            sv.remove_prefix(1);
        }
        while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '\n'))
        {
            sv.remove_suffix(1);
        }
        return sv;
    }

    struct TerminalSessionViewModel::TerminalTabState
    {
        int id = 0;
        QString title;
        QString repoRootPath;
        QString currentWorkingDir;
        QString previousWorkingDir;
        QString branch;
        QString shellId;
        QString accentColor;
        std::unique_ptr<TerminalOutputListModel> outputModel;
        std::unique_ptr<TerminalOutputListModel> pagerModel;
        QStringList history;
        int historyCursor = -1; // -1 means not browsing history
        QString historyDraft;
        QString inputDraft;
        int inputCursorPos = 0;

        bool isRunning = false;
        int activeCommandHeaderId = 0;
        bool activeCommandWasGit = false;
        bool activeCommandCanPage = false;
        bool pagerActive = false;
        bool closedPagerEarly = false;
        QString pagerCommand;
        std::shared_ptr<gf::terminal::ports::ITerminalProcessHandle> activeProcess;

        std::string stdoutPending;
        std::string stderrPending;
        bool stdoutOverwriteLast = false;
        bool stderrOverwriteLast = false;
        gf::terminal::bo::TerminalTextStyle stdoutStyle{};
        gf::terminal::bo::TerminalTextStyle stderrStyle{};
    };

    struct TerminalSessionViewModel::RepoTerminalBucket
    {
        QString repoId;
        QString repoName;
        QString repoPath;
        QString branch;
        std::vector<std::unique_ptr<TerminalTabState>> tabs;
        int activeTabIndex = -1;
        int nextTabNumber = 1;
    };
} // namespace gf::desktop
