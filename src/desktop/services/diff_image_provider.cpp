#include "desktop/services/diff_image_provider.hpp"

#include <QStringList>

namespace gf::desktop
{
    DiffImageStore& DiffImageStore::instance()
    {
        static DiffImageStore store;
        return store;
    }

    std::uint64_t DiffImageStore::allocateInstanceId()
    {
        std::lock_guard lock(m_mutex);
        return m_nextInstanceId++;
    }

    std::uint64_t DiffImageStore::setImages(std::uint64_t instanceId, QImage oldImage, QImage newImage,
                                            QImage diffImage)
    {
        std::lock_guard lock(m_mutex);
        auto& entry = m_entries[instanceId];
        entry.generation += 1;
        entry.oldImage = std::move(oldImage);
        entry.newImage = std::move(newImage);
        entry.diffImage = std::move(diffImage);
        return entry.generation;
    }

    void DiffImageStore::clear(std::uint64_t instanceId)
    {
        std::lock_guard lock(m_mutex);
        m_entries.erase(instanceId);
    }

    QImage DiffImageStore::get(std::uint64_t instanceId, std::uint64_t generation, const QString& slot) const
    {
        std::lock_guard lock(m_mutex);
        const auto it = m_entries.find(instanceId);
        if (it == m_entries.end() || it->second.generation != generation)
        {
            return {};
        }

        if (slot == QLatin1String("old")) return it->second.oldImage;
        if (slot == QLatin1String("new")) return it->second.newImage;
        if (slot == QLatin1String("diff")) return it->second.diffImage;
        return {};
    }

    QString DiffImageStore::buildUrl(std::uint64_t instanceId, std::uint64_t generation, const QString& slot)
    {
        return QStringLiteral("image://diffpreview/%1/%2/%3")
               .arg(QString::number(instanceId))
               .arg(QString::number(generation))
               .arg(slot);
    }

    DiffImageProvider::DiffImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }

    QImage DiffImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
    {
        const QStringList parts = id.split(QLatin1Char('/'));
        if (parts.size() < 3)
        {
            if (size) *size = QSize();
            return {};
        }

        bool okInstance = false;
        bool okGen = false;
        const std::uint64_t instanceId = parts[0].toULongLong(&okInstance);
        const std::uint64_t generation = parts[1].toULongLong(&okGen);
        if (!okInstance || !okGen)
        {
            if (size) *size = QSize();
            return {};
        }

        QImage img = DiffImageStore::instance().get(instanceId, generation, parts[2]);
        if (size)
        {
            *size = img.size();
        }
        if (!img.isNull() && requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0)
        {
            return img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        return img;
    }
} // namespace gf::desktop
