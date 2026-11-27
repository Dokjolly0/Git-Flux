#include <catch2/catch_test_macros.hpp>

#include "core/modules/terminal/bl/git_catalog_service.hpp"
#include "core/modules/terminal/bl/git_completion_engine.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/terminal/native_terminal_process_runner.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>

using namespace gf::terminal::bl;
using namespace gf::terminal::bo;

TEST_CASE("GitCatalogService dynamically discovers full installed Git command set and flags", "[integration][terminal][catalog]")
{
    const auto discovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(discovery.has_value());
    const auto gitExe = discovery->directBinaryPath.empty() ? discovery->discoveredInPath : discovery->directBinaryPath;

    gf::infra::git::NativeGitCommandExecutor executor(gitExe);
    GitCatalogService catalog(&executor);
    catalog.discoverAllCommands();

    REQUIRE(catalog.isDiscovered());
    const auto all = catalog.allCommands();
    // Real Git installations expose well over 100 porcelain + ancillary + plumbing commands
    REQUIRE(all.size() >= 100);

    auto hasCmd = [&](const std::string_view name)
    {
        return std::ranges::any_of(all, [&](const GitCommandSpec& c) { return c.name == name; });
    };

    REQUIRE(hasCmd("checkout"));
    REQUIRE(hasCmd("commit"));
    REQUIRE(hasCmd("worktree"));
    REQUIRE(hasCmd("bisect"));
    REQUIRE(hasCmd("cat-file"));
    REQUIRE(hasCmd("rev-parse"));
    REQUIRE(hasCmd("check-ignore"));

    // Verify progressive completion for `git c` includes porcelain AND plumbing commands
    const auto cRes = GitCompletionEngine::complete("git c", 5, {}, catalog, {});
    REQUIRE(cRes.items.size() >= 12);
    REQUIRE(std::ranges::any_of(cRes.items, [](const CompletionItem& it) { return it.displayText == "checkout"; }));
    REQUIRE(std::ranges::any_of(cRes.items, [](const CompletionItem& it) { return it.displayText == "cat-file"; }));

    // Verify on-demand option discovery for `commit`
    const auto commitOpts = catalog.loadOptionsFor("commit");
    REQUIRE(commitOpts.size() >= 10);
    REQUIRE(std::ranges::any_of(commitOpts, [](const GitOptionSpec& o) { return o.longFlag == "--amend"; }));
    REQUIRE(std::ranges::any_of(commitOpts, [](const GitOptionSpec& o) { return o.longFlag == "--message"; }));
}

TEST_CASE("NativeTerminalProcessRunner streams Git and shell output in target working directory", "[integration][terminal][runner]")
{
    const auto discovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(discovery.has_value());
    const auto gitExe = discovery->directBinaryPath.empty() ? discovery->discoveredInPath : discovery->directBinaryPath;

    gf::infra::terminal::NativeTerminalProcessRunner runner(gitExe);
    REQUIRE_FALSE(runner.availableShells().empty());

    const auto workDir = std::filesystem::current_path();

    SECTION("Direct Git fast-path streams output and reports exit code 0")
    {
        std::mutex mu;
        std::string capturedOut;
        TerminalCommandResult finalRes;
        std::atomic<bool> done{false};

        TerminalExecutionRequest req;
        req.commandLine = "git --version";
        req.workingDirectory = workDir;

        TerminalExecutionCallbacks cb;
        cb.onOutput = [&](const TerminalOutputStream stream, const std::string_view chunk)
        {
            if (stream == TerminalOutputStream::Stdout)
            {
                std::scoped_lock lock(mu);
                capturedOut.append(chunk);
            }
        };
        cb.onFinished = [&](const TerminalCommandResult& res)
        {
            finalRes = res;
            done.store(true, std::memory_order_release);
        };

        auto handle = runner.start(req, std::move(cb));
        REQUIRE(handle.has_value());
        (*handle)->wait();

        REQUIRE(done.load(std::memory_order_acquire));
        REQUIRE(finalRes.exitCode == 0);
        REQUIRE_FALSE(finalRes.wasCancelled);
        REQUIRE(capturedOut.find("git version") != std::string::npos);
    }

    SECTION("Non-git shell command executes cleanly")
    {
        std::mutex mu;
        std::string capturedOut;
        TerminalCommandResult finalRes;

        TerminalExecutionRequest req;
        req.commandLine = "echo gitflux_cli_ok";
        req.workingDirectory = workDir;

        TerminalExecutionCallbacks cb;
        cb.onOutput = [&](const TerminalOutputStream stream, const std::string_view chunk)
        {
            if (stream == TerminalOutputStream::Stdout)
            {
                std::scoped_lock lock(mu);
                capturedOut.append(chunk);
            }
        };
        cb.onFinished = [&](const TerminalCommandResult& res)
        {
            finalRes = res;
        };

        auto handle = runner.start(req, std::move(cb));
        REQUIRE(handle.has_value());
        (*handle)->wait();

        REQUIRE(finalRes.exitCode == 0);
        REQUIRE(capturedOut.find("gitflux_cli_ok") != std::string::npos);
    }

    SECTION("Shell command changing directory reports updated finalWorkingDirectory")
    {
        if (std::filesystem::exists(workDir / "scripts"))
        {
            TerminalCommandResult finalRes;
            TerminalExecutionRequest req;
            req.commandLine = "cd scripts/ && echo cwd_tracked";
            req.workingDirectory = workDir;

            TerminalExecutionCallbacks cb;
            cb.onFinished = [&](const TerminalCommandResult& res)
            {
                finalRes = res;
            };

            auto handle = runner.start(req, std::move(cb));
            REQUIRE(handle.has_value());
            (*handle)->wait();

            REQUIRE(finalRes.exitCode == 0);
            REQUIRE(finalRes.finalWorkingDirectory.has_value());
            std::error_code ec;
            REQUIRE(std::filesystem::equivalent(*finalRes.finalWorkingDirectory, workDir / "scripts", ec));
        }
    }
}
