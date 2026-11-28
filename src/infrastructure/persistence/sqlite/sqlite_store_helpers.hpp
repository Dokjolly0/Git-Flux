#pragma once

#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_statement.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace gf::infra::persistence::sqlite
{
    template <typename ErrorT>
    [[nodiscard]] ErrorT makeStorageError(const SqliteError& err)
    {
        return ErrorT{
            .kind = ErrorT::Kind::StorageError,
            .message = err.message
        };
    }

    template <typename IdT, typename ErrorT>
    [[nodiscard]] std::expected<std::vector<IdT>, ErrorT>
    loadOrderedRelationIds(SqliteConnection& conn,
                           const std::string_view sql,
                           const std::string_view parentId)
    {
        auto stmtRes = SqliteStatement::prepare(conn, sql);
        if (!stmtRes)
        {
            return std::unexpected(makeStorageError<ErrorT>(stmtRes.error()));
        }

        if (const auto bindRes = stmtRes->bindText(1, parentId); !bindRes)
        {
            return std::unexpected(makeStorageError<ErrorT>(bindRes.error()));
        }

        std::vector<IdT> ids;
        while (true)
        {
            auto stepRes = stmtRes->step();
            if (!stepRes)
            {
                return std::unexpected(makeStorageError<ErrorT>(stepRes.error()));
            }
            if (!*stepRes)
            {
                break;
            }
            ids.emplace_back(stmtRes->getText(0));
        }
        return ids;
    }

    template <typename IdT, typename ErrorT>
    [[nodiscard]] std::expected<void, ErrorT>
    replaceOrderedRelationIds(SqliteConnection& conn,
                              const std::string_view deleteSql,
                              const std::string_view insertSql,
                              const std::string_view parentId,
                              const std::vector<IdT>& ids,
                              const std::string_view fkErrorMessage)
    {
        auto delStmtRes = SqliteStatement::prepare(conn, deleteSql);
        if (!delStmtRes)
        {
            return std::unexpected(makeStorageError<ErrorT>(delStmtRes.error()));
        }
        if (const auto bindRes = delStmtRes->bindText(1, parentId); !bindRes)
        {
            return std::unexpected(makeStorageError<ErrorT>(bindRes.error()));
        }
        if (const auto delStepRes = delStmtRes->step(); !delStepRes)
        {
            return std::unexpected(makeStorageError<ErrorT>(delStepRes.error()));
        }

        if (!ids.empty())
        {
            auto insStmtRes = SqliteStatement::prepare(conn, insertSql);
            if (!insStmtRes)
            {
                return std::unexpected(makeStorageError<ErrorT>(insStmtRes.error()));
            }

            for (int i = 0; i < static_cast<int>(ids.size()); ++i)
            {
                insStmtRes->reset();
                insStmtRes->clearBindings();
                if (const auto bindRes = insStmtRes->bindText(1, parentId); !bindRes)
                {
                    return std::unexpected(makeStorageError<ErrorT>(bindRes.error()));
                }
                if (const auto bindRes = insStmtRes->bindText(2, ids[static_cast<std::size_t>(i)].value()); !bindRes)
                {
                    return std::unexpected(makeStorageError<ErrorT>(bindRes.error()));
                }
                if (const auto bindRes = insStmtRes->bindInt(3, i); !bindRes)
                {
                    return std::unexpected(makeStorageError<ErrorT>(bindRes.error()));
                }
                if (const auto insStep = insStmtRes->step(); !insStep)
                {
                    return std::unexpected(ErrorT{
                        .kind = ErrorT::Kind::StorageError,
                        .message = std::string(fkErrorMessage)
                    });
                }
            }
        }
        return {};
    }

    template <typename ErrorT, typename MapErrorFn>
    [[nodiscard]] std::expected<bool, ErrorT>
    deleteRowById(SqliteConnection& conn,
                  const std::string_view sql,
                  const std::string_view id,
                  MapErrorFn&& mapErr)
    {
        auto stmtRes = SqliteStatement::prepare(conn, sql);
        if (!stmtRes)
        {
            return std::unexpected(mapErr(stmtRes.error()));
        }
        if (const auto bindRes = stmtRes->bindText(1, id); !bindRes)
        {
            return std::unexpected(mapErr(bindRes.error()));
        }
        if (const auto stepRes = stmtRes->step(); !stepRes)
        {
            return std::unexpected(mapErr(stepRes.error()));
        }
        return conn.changes() > 0;
    }
} // namespace gf::infra::persistence::sqlite
