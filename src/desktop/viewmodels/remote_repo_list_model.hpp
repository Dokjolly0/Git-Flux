#pragma once

#include "core/modules/git/bo/git_provider_profile.hpp"
#include <QAbstractListModel>
#include <QString>
#include <vector>

namespace gf::desktop::viewmodels
{
    class RemoteRepoListModel : public QAbstractListModel
    {
        Q_OBJECT

        Q_PROPERTY(QString searchFilter READ searchFilter WRITE setSearchFilter NOTIFY searchFilterChanged)
        Q_PROPERTY(
            QString selectedProtocol READ selectedProtocol WRITE setSelectedProtocol NOTIFY selectedProtocolChanged)
        Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)
        Q_PROPERTY(int filteredCount READ filteredCount NOTIFY countChanged)

    public:
        enum RepoRoles
        {
            NameRole = Qt::UserRole + 1,
            FullNameRole,
            DescriptionRole,
            IsPrivateRole,
            DefaultBranchRole,
            HttpsUrlRole,
            SshUrlRole,
            ActiveCloneUrlRole,
            UpdatedAtRole,
            StarsCountRole
        };

        Q_ENUM(RepoRoles)

        explicit RemoteRepoListModel(QObject* parent = nullptr);
        ~RemoteRepoListModel() override = default;

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        [[nodiscard]] QString searchFilter() const noexcept { return m_searchFilter; }
        void setSearchFilter(const QString& filter);

        [[nodiscard]] QString selectedProtocol() const noexcept { return m_selectedProtocol; }
        void setSelectedProtocol(const QString& proto);

        [[nodiscard]] int totalCount() const noexcept { return static_cast<int>(m_allRepos.size()); }
        [[nodiscard]] int filteredCount() const noexcept { return static_cast<int>(m_filteredIndices.size()); }

        void setRepositories(std::vector<core::git::bo::RemoteRepositoryItem> repos);
        void clear();

    Q_INVOKABLE QVariantMap getRepoAt(int index) const;

        signals :

        void searchFilterChanged();
        void selectedProtocolChanged();
        void countChanged();

    private:
        std::vector<core::git::bo::RemoteRepositoryItem> m_allRepos;
        std::vector<size_t> m_filteredIndices;
        QString m_searchFilter;
        QString m_selectedProtocol{QStringLiteral("ssh")};

        void applyFilter();
    };
} // namespace gf::desktop::viewmodels
