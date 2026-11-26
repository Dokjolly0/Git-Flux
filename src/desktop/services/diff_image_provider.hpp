#pragma once

#include <QImage>
#include <QQuickImageProvider>
#include <QString>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace gf::desktop
{
    /// Thread-safe in-memory store holding decoded Before / After / Pixel-Diff images
    /// for each DiffViewModel instance so QML can load them via `image://diffpreview/...`
    /// without Base64 encoding or temporary files.
    class DiffImageStore final
    {
    public:
        struct Entry
        {
            std::uint64_t generation = 0;
            QImage oldImage;
            QImage newImage;
            QImage diffImage;
        };

        static DiffImageStore& instance();

        [[nodiscard]] std::uint64_t allocateInstanceId();

        std::uint64_t setImages(std::uint64_t instanceId, QImage oldImage, QImage newImage, QImage diffImage);
        void clear(std::uint64_t instanceId);
        [[nodiscard]] QImage get(std::uint64_t instanceId, std::uint64_t generation, const QString& slot) const;

        [[nodiscard]] static QString buildUrl(std::uint64_t instanceId, std::uint64_t generation, const QString& slot);

    private:
        DiffImageStore() = default;

        mutable std::mutex m_mutex;
        std::uint64_t m_nextInstanceId = 1;
        std::uint64_t m_nextGeneration = 1;
        std::unordered_map<std::uint64_t, Entry> m_entries;
    };

    /// QML image provider registered under `"diffpreview"` (`image://diffpreview/<instance>/<gen>/<slot>`).
    class DiffImageProvider final : public QQuickImageProvider
    {
    public:
        DiffImageProvider();
        QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
    };
} // namespace gf::desktop
