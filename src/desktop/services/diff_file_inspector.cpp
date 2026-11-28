#include "desktop/services/diff_file_inspector.hpp"

#include <QBuffer>
#include <QCryptographicHash>
#include <QImageReader>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

namespace gf::desktop
{
    namespace
    {
        std::string lowercaseExtension(const std::string& path)
        {
            const auto slash = path.find_last_of("/\\");
            const std::string_view name = (slash == std::string::npos)
                                              ? std::string_view(path)
                                              : std::string_view(path).substr(slash + 1);
            const auto dot = name.rfind('.');
            if (dot == std::string_view::npos || dot + 1 >= name.size())
            {
                return {};
            }
            std::string ext(name.substr(dot + 1));
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return ext;
        }

        bool hasMagic(const std::string& bytes, std::string_view magic, std::size_t offset = 0)
        {
            if (bytes.size() < offset + magic.size()) return false;
            return std::string_view(bytes.data() + offset, magic.size()) == magic;
        }

        std::uint16_t readLE16(const std::string& b, std::size_t off)
        {
            return static_cast<std::uint16_t>(static_cast<unsigned char>(b[off])) |
                   (static_cast<std::uint16_t>(static_cast<unsigned char>(b[off + 1])) << 8);
        }

        std::uint32_t readLE32(const std::string& b, std::size_t off)
        {
            return static_cast<std::uint32_t>(static_cast<unsigned char>(b[off])) |
                   (static_cast<std::uint32_t>(static_cast<unsigned char>(b[off + 1])) << 8) |
                   (static_cast<std::uint32_t>(static_cast<unsigned char>(b[off + 2])) << 16) |
                   (static_cast<std::uint32_t>(static_cast<unsigned char>(b[off + 3])) << 24);
        }

        std::uint16_t readBE16(const std::string& b, std::size_t off)
        {
            return (static_cast<std::uint16_t>(static_cast<unsigned char>(b[off])) << 8) |
                   static_cast<std::uint16_t>(static_cast<unsigned char>(b[off + 1]));
        }

        std::uint32_t readBE32(const std::string& b, std::size_t off)
        {
            return (static_cast<std::uint32_t>(static_cast<unsigned char>(b[off])) << 24) |
                   (static_cast<std::uint32_t>(static_cast<unsigned char>(b[off + 1])) << 16) |
                   (static_cast<std::uint32_t>(static_cast<unsigned char>(b[off + 2])) << 8) |
                   static_cast<std::uint32_t>(static_cast<unsigned char>(b[off + 3]));
        }

        QString computeSha256Hex(const std::string& bytes)
        {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(QByteArrayView(bytes.data(), static_cast<qsizetype>(bytes.size())));
            return QString::fromLatin1(hash.result().toHex());
        }

        void appendMetaRow(QVariantList& rows, const QString& label, const QString& oldVal, const QString& newVal)
        {
            if (oldVal.isEmpty() && newVal.isEmpty()) return;
            QVariantMap m;
            m[QStringLiteral("label")] = label;
            m[QStringLiteral("oldValue")] = oldVal;
            m[QStringLiteral("newValue")] = newVal;
            m[QStringLiteral("changed")] = (!oldVal.isEmpty() && !newVal.isEmpty() && oldVal != newVal);
            rows.append(m);
        }

        struct DecodedImage
        {
            QImage image;
            int width = 0;
            int height = 0;
            int depth = 0;
            bool hasAlpha = false;
        };

        DecodedImage tryDecodeImage(const std::string& bytes, const std::string& ext)
        {
            DecodedImage res;
            if (bytes.empty()) return res;

            QByteArray ba(bytes.data(), static_cast<qsizetype>(bytes.size()));
            QBuffer buf(&ba);
            buf.open(QIODevice::ReadOnly);

            QImageReader reader(&buf);
            reader.setAutoTransform(true);
            reader.setDecideFormatFromContent(true);
            if (ext == "svg" || ext == "svgz")
            {
                reader.setFormat("svg");
            }
            if (!reader.canRead())
            {
                return res;
            }

            // Cap rasterization size for vector/PDF formats so SVGs render crisply without huge allocations.
            const QByteArray detectedFmt = reader.format().toUpper();
            if (detectedFmt == "SVG" || detectedFmt == "SVGZ" || detectedFmt == "PDF")
            {
                QSize sz = reader.size();
                if (sz.isValid() && sz.width() > 0 && sz.height() > 0)
                {
                    if (sz.width() < 256 && sz.height() < 256)
                    {
                        sz.scale(512, 512, Qt::KeepAspectRatio);
                        reader.setScaledSize(sz);
                    }
                    else if (sz.width() > 1600 || sz.height() > 1600)
                    {
                        sz.scale(1600, 1600, Qt::KeepAspectRatio);
                        reader.setScaledSize(sz);
                    }
                }
                else
                {
                    reader.setScaledSize(QSize(512, 512));
                }
            }

            QImage img = reader.read();
            if (img.isNull())
            {
                return res;
            }

            res.width = img.width();
            res.height = img.height();
            res.depth = img.depth();
            res.hasAlpha = img.hasAlphaChannel();
            res.image = std::move(img);
            return res;
        }

        void computePixelDiff(const QImage& oldImg, const QImage& newImg,
                              QImage& outDiff, double& outPercent)
        {
            if (oldImg.isNull() || newImg.isNull()) return;

            constexpr int kMaxDiffDim = 1600;
            QImage a = oldImg;
            QImage b = newImg;
            if (a.width() > kMaxDiffDim || a.height() > kMaxDiffDim)
            {
                a = a.scaled(kMaxDiffDim, kMaxDiffDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            if (b.width() > kMaxDiffDim || b.height() > kMaxDiffDim)
            {
                b = b.scaled(kMaxDiffDim, kMaxDiffDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }

            a = a.convertToFormat(QImage::Format_RGBA8888);
            b = b.convertToFormat(QImage::Format_RGBA8888);

            const int w = std::max(a.width(), b.width());
            const int h = std::max(a.height(), b.height());
            QImage diff(w, h, QImage::Format_RGBA8888);

            qint64 changed = 0;
            const qint64 total = static_cast<qint64>(w) * static_cast<qint64>(h);

            for (int y = 0; y < h; ++y)
            {
                const uchar* rowA = (y < a.height()) ? a.constScanLine(y) : nullptr;
                const uchar* rowB = (y < b.height()) ? b.constScanLine(y) : nullptr;
                uchar* rowD = diff.scanLine(y);

                for (int x = 0; x < w; ++x)
                {
                    const bool inA = (rowA != nullptr && x < a.width());
                    const bool inB = (rowB != nullptr && x < b.width());

                    const uchar ar = inA ? rowA[x * 4 + 0] : 0;
                    const uchar ag = inA ? rowA[x * 4 + 1] : 0;
                    const uchar ab = inA ? rowA[x * 4 + 2] : 0;
                    const uchar aa = inA ? rowA[x * 4 + 3] : 0;

                    const uchar br = inB ? rowB[x * 4 + 0] : 0;
                    const uchar bg = inB ? rowB[x * 4 + 1] : 0;
                    const uchar bb = inB ? rowB[x * 4 + 2] : 0;
                    const uchar ba = inB ? rowB[x * 4 + 3] : 0;

                    const int delta = std::abs(int(ar) - int(br)) + std::abs(int(ag) - int(bg)) +
                                      std::abs(int(ab) - int(bb)) + std::abs(int(aa) - int(ba));

                    const bool pixelChanged = (!inA || !inB || delta > 4);
                    if (pixelChanged)
                    {
                        ++changed;
                        // Vivid rose highlight (#F43F5E) so changed pixels pop immediately
                        rowD[x * 4 + 0] = 244;
                        rowD[x * 4 + 1] = 63;
                        rowD[x * 4 + 2] = 94;
                        rowD[x * 4 + 3] = 235;
                    }
                    else
                    {
                        // Subtle dimmed grayscale context
                        const uchar lum = static_cast<uchar>((int(br) * 77 + int(bg) * 150 + int(bb) * 29) >> 8);
                        const uchar dimmed = static_cast<uchar>(lum / 3);
                        rowD[x * 4 + 0] = dimmed;
                        rowD[x * 4 + 1] = dimmed;
                        rowD[x * 4 + 2] = dimmed;
                        rowD[x * 4 + 3] = static_cast<uchar>(ba / 2);
                    }
                }
            }

            outDiff = std::move(diff);
            outPercent = (total > 0) ? (100.0 * static_cast<double>(changed) / static_cast<double>(total)) : 0.0;
        }

        // Lightweight ZIP Central Directory reader (supports .zip, .jar, .apk, .docx, .xlsx, .pptx, .epub, .whl...)
        struct ZipEntryInfo
        {
            std::uint32_t crc32 = 0;
            std::uint32_t uncompressedSize = 0;
        };

        std::map<std::string, ZipEntryInfo> parseZipCentralDirectory(const std::string& bytes)
        {
            std::map<std::string, ZipEntryInfo> entries;
            if (bytes.size() < 22) return entries;

            // Locate End of Central Directory record (0x06054b50) within last 65557 bytes
            const std::size_t searchStart = (bytes.size() > 65557) ? (bytes.size() - 65557) : 0;
            std::size_t eocdPos = std::string::npos;
            for (std::size_t pos = bytes.size() - 22;; --pos)
            {
                if (readLE32(bytes, pos) == 0x06054b50u)
                {
                    eocdPos = pos;
                    break;
                }
                if (pos == searchStart) break;
            }
            if (eocdPos == std::string::npos) return entries;

            const std::uint16_t totalEntries = readLE16(bytes, eocdPos + 10);
            const std::uint32_t cdOffset = readLE32(bytes, eocdPos + 16);
            if (cdOffset >= bytes.size()) return entries;

            std::size_t cur = cdOffset;
            constexpr std::size_t kMaxEntries = 500;
            const std::size_t count = std::min<std::size_t>(totalEntries, kMaxEntries);
            for (std::size_t i = 0; i < count; ++i)
            {
                if (cur + 46 > bytes.size()) break;
                if (readLE32(bytes, cur) != 0x02014b50u) break;

                const std::uint32_t crc = readLE32(bytes, cur + 16);
                const std::uint32_t uncompSize = readLE32(bytes, cur + 24);
                const std::uint16_t nameLen = readLE16(bytes, cur + 28);
                const std::uint16_t extraLen = readLE16(bytes, cur + 30);
                const std::uint16_t commentLen = readLE16(bytes, cur + 32);

                const std::size_t nameStart = cur + 46;
                if (nameStart + nameLen > bytes.size()) break;
                std::string name(bytes.data() + nameStart, nameLen);
                if (!name.empty() && name.back() != '/')
                {
                    entries[std::move(name)] = ZipEntryInfo{
                        .crc32 = crc,
                        .uncompressedSize = uncompSize
                    };
                }
                cur = nameStart + nameLen + extraLen + commentLen;
            }
            return entries;
        }

        // Format-specific metadata sniffers
        struct PdfInfo
        {
            QString version;
            int pages = 0;
            QString title;
            QString author;
        };

        QString extractPdfInfoField(const std::string& bytes, std::string_view key)
        {
            const std::size_t pos = bytes.find(key);
            if (pos == std::string::npos) return {};
            std::size_t i = pos + key.size();
            while (i < bytes.size() && (bytes[i] == ' ' || bytes[i] == '\t' || bytes[i] == '\r' || bytes[i] == '\n'))
            {
                ++i;
            }
            if (i >= bytes.size() || bytes[i] != '(') return {};
            ++i;
            std::string val;
            int depth = 1;
            while (i < bytes.size() && depth > 0 && val.size() < 200)
            {
                const char c = bytes[i++];
                if (c == '\\' && i < bytes.size())
                {
                    val.push_back(bytes[i++]);
                    continue;
                }
                if (c == '(') ++depth;
                else if (c == ')')
                {
                    --depth;
                    if (depth == 0) break;
                }
                if (static_cast<unsigned char>(c) >= 32) val.push_back(c);
            }
            return QString::fromUtf8(val.data(), static_cast<qsizetype>(val.size())).trimmed();
        }

        PdfInfo inspectPdf(const std::string& bytes)
        {
            PdfInfo info;
            if (!hasMagic(bytes, "%PDF-")) return info;
            const std::size_t nl = bytes.find_first_of("\r\n", 5);
            if (nl != std::string::npos && nl <= 16)
            {
                info.version = QStringLiteral("PDF ") +
                               QString::fromLatin1(bytes.data() + 5, static_cast<qsizetype>(nl - 5));
            }
            // Count `/Type /Page` (not `/Pages`) occurrences cleanly
            std::size_t pos = 0;
            while ((pos = bytes.find("/Type", pos)) != std::string::npos)
            {
                pos += 5;
                while (pos < bytes.size() && (bytes[pos] == ' ' || bytes[pos] == '\t' || bytes[pos] == '\r' || bytes[pos] == '\n'))
                {
                    ++pos;
                }
                if (pos + 5 <= bytes.size() && std::string_view(bytes.data() + pos, 5) == "/Page")
                {
                    const char next = (pos + 5 < bytes.size()) ? bytes[pos + 5] : ' ';
                    if (next != 's' && next != 'S' && !std::isalpha(static_cast<unsigned char>(next)))
                    {
                        ++info.pages;
                    }
                }
            }
            info.title = extractPdfInfoField(bytes, "/Title");
            info.author = extractPdfInfoField(bytes, "/Author");
            return info;
        }

        struct WavInfo
        {
            bool valid = false;
            int channels = 0;
            std::uint32_t sampleRate = 0;
            int bitsPerSample = 0;
            double durationSec = 0.0;
        };

        WavInfo inspectWav(const std::string& bytes)
        {
            WavInfo w;
            if (bytes.size() < 44 || !hasMagic(bytes, "RIFF", 0) || !hasMagic(bytes, "WAVE", 8)) return w;
            std::size_t pos = 12;
            std::uint32_t dataBytes = 0;
            std::uint32_t byteRate = 0;
            while (pos + 8 <= bytes.size())
            {
                const std::string_view chunkId(bytes.data() + pos, 4);
                const std::uint32_t chunkSize = readLE32(bytes, pos + 4);
                pos += 8;
                if (pos + chunkSize > bytes.size()) break;
                if (chunkId == "fmt " && chunkSize >= 16)
                {
                    w.channels = readLE16(bytes, pos + 2);
                    w.sampleRate = readLE32(bytes, pos + 4);
                    byteRate = readLE32(bytes, pos + 8);
                    w.bitsPerSample = readLE16(bytes, pos + 14);
                    w.valid = true;
                }
                else if (chunkId == "data")
                {
                    dataBytes = chunkSize;
                }
                pos += chunkSize + (chunkSize & 1u);
            }
            if (w.valid && byteRate > 0 && dataBytes > 0)
            {
                w.durationSec = static_cast<double>(dataBytes) / static_cast<double>(byteRate);
            }
            return w;
        }

        QString inspectSqliteSummary(const std::string& bytes)
        {
            if (bytes.size() < 100 || !hasMagic(bytes, "SQLite format 3", 0)) return {};
            std::uint32_t pageSize = readBE16(bytes, 16);
            if (pageSize == 1) pageSize = 65536;
            const std::uint32_t pageCount = readBE32(bytes, 28);
            const std::uint32_t encoding = readBE32(bytes, 56);
            const QString encStr = (encoding == 1) ? QStringLiteral("UTF-8")
                                 : (encoding == 2) ? QStringLiteral("UTF-16le")
                                 : (encoding == 3) ? QStringLiteral("UTF-16be")
                                                   : QStringLiteral("Standard");
            return QStringLiteral("Page %1 B • %2 pagine • %3")
                   .arg(pageSize)
                   .arg(pageCount)
                   .arg(encStr);
        }

        QString inspectElfSummary(const std::string& bytes)
        {
            if (bytes.size() < 20 || !hasMagic(bytes, "\x7f" "ELF", 0)) return {};
            const int bits = (bytes[4] == 2) ? 64 : 32;
            const bool le = (bytes[5] == 1);
            const std::uint16_t type = le ? readLE16(bytes, 16) : readBE16(bytes, 16);
            const std::uint16_t mach = le ? readLE16(bytes, 18) : readBE16(bytes, 18);
            const QString typeStr = (type == 2) ? QStringLiteral("Eseguibile")
                                  : (type == 3) ? QStringLiteral("Shared Object / PIE")
                                  : (type == 1) ? QStringLiteral("Relocatable")
                                                : QStringLiteral("ELF");
            const QString machStr = (mach == 62)  ? QStringLiteral("x86_64")
                                  : (mach == 183) ? QStringLiteral("ARM64")
                                  : (mach == 3)   ? QStringLiteral("x86")
                                  : (mach == 40)  ? QStringLiteral("ARM")
                                  : (mach == 243) ? QStringLiteral("RISC-V")
                                                  : QStringLiteral("Arch #%1").arg(mach);
            return QStringLiteral("ELF %1-bit %2 (%3)").arg(bits).arg(machStr, typeStr);
        }

        void buildHexRows(const std::string& oldBytes, bool hasOld,
                          const std::string& newBytes, bool hasNew,
                          qint64 oldTotal, qint64 newTotal,
                          QVariantList& outRows, bool& outTruncated, int& outChangedRows)
        {
            const std::size_t oldLen = std::min(oldBytes.size(), DiffFileInspector::kHexPreviewBytes);
            const std::size_t newLen = std::min(newBytes.size(), DiffFileInspector::kHexPreviewBytes);
            const std::size_t maxLen = std::max(oldLen, newLen);
            outTruncated = (static_cast<std::size_t>(std::max<qint64>(oldTotal, newTotal)) >
                            DiffFileInspector::kHexPreviewBytes);

            if (maxLen == 0) return;

            const std::size_t rowCount = (maxLen + 15) / 16;
            outRows.reserve(static_cast<qsizetype>(rowCount));

            static constexpr char kHex[] = "0123456789ABCDEF";
            auto byteToHex = [](unsigned char v) -> QString
            {
                QChar chars[2] = {QChar::fromLatin1(kHex[v >> 4]), QChar::fromLatin1(kHex[v & 0x0F])};
                return QString(chars, 2);
            };

            for (std::size_t row = 0; row < rowCount; ++row)
            {
                const std::size_t base = row * 16;
                QString oldHex, newHex, oldRich, newRich, oldAscii, newAscii;
                bool rowDiff = false;

                for (std::size_t i = 0; i < 16; ++i)
                {
                    if (i == 8)
                    {
                        oldHex += QLatin1Char(' ');
                        newHex += QLatin1Char(' ');
                        oldRich += QLatin1Char(' ');
                        newRich += QLatin1Char(' ');
                    }
                    const std::size_t idx = base + i;
                    const bool inOld = hasOld && idx < oldLen;
                    const bool inNew = hasNew && idx < newLen;
                    const unsigned char ob = inOld ? static_cast<unsigned char>(oldBytes[idx]) : 0;
                    const unsigned char nb = inNew ? static_cast<unsigned char>(newBytes[idx]) : 0;
                    const bool byteDiff = (hasOld && hasNew) ? (!inOld || !inNew || ob != nb) : false;
                    if (byteDiff) rowDiff = true;

                    if (inOld)
                    {
                        const QString h = byteToHex(ob);
                        oldHex += h + QLatin1Char(' ');
                        if (byteDiff)
                        {
                            oldRich += QStringLiteral("<span style='background-color:#5C1D24;color:#FDA4AF;font-weight:bold;'>") +
                                       h + QStringLiteral("</span> ");
                        }
                        else
                        {
                            oldRich += h + QLatin1Char(' ');
                        }
                        oldAscii += (ob >= 32 && ob <= 126) ? QChar(ob) : QChar('.');
                    }
                    else if (hasOld)
                    {
                        oldHex += QStringLiteral("   ");
                        oldRich += QStringLiteral("   ");
                        oldAscii += QLatin1Char(' ');
                    }

                    if (inNew)
                    {
                        const QString h = byteToHex(nb);
                        newHex += h + QLatin1Char(' ');
                        if (byteDiff)
                        {
                            newRich += QStringLiteral("<span style='background-color:#143B25;color:#86EFAC;font-weight:bold;'>") +
                                       h + QStringLiteral("</span> ");
                        }
                        else
                        {
                            newRich += h + QLatin1Char(' ');
                        }
                        newAscii += (nb >= 32 && nb <= 126) ? QChar(nb) : QChar('.');
                    }
                    else if (hasNew)
                    {
                        newHex += QStringLiteral("   ");
                        newRich += QStringLiteral("   ");
                        newAscii += QLatin1Char(' ');
                    }
                }

                if (rowDiff) ++outChangedRows;

                QVariantMap m;
                m[QStringLiteral("offset")] = QStringLiteral("%1").arg(static_cast<quint64>(base), 8, 16, QLatin1Char('0')).toUpper();
                m[QStringLiteral("oldHex")] = oldHex.trimmed();
                m[QStringLiteral("newHex")] = newHex.trimmed();
                m[QStringLiteral("oldRichHex")] = oldRich.trimmed();
                m[QStringLiteral("newRichHex")] = newRich.trimmed();
                m[QStringLiteral("oldAscii")] = oldAscii;
                m[QStringLiteral("newAscii")] = newAscii;
                m[QStringLiteral("hasDiff")] = rowDiff;
                outRows.append(m);
            }
        }

        struct FormatClassification
        {
            QString kind;
            QString badge;
            QString description;
            bool isZipBased = false;
        };

        FormatClassification classifyFile(const std::string& ext, const std::string& sampleBytes)
        {
            if (ext == "svg" || ext == "svgz")
                return {QStringLiteral("image"), QStringLiteral("SVG"), QStringLiteral("Grafica vettoriale SVG"), false};
            if (ext == "png" || hasMagic(sampleBytes, "\x89PNG\r\n\x1a\n"))
                return {QStringLiteral("image"), QStringLiteral("PNG"), QStringLiteral("Immagine PNG"), false};
            if (ext == "jpg" || ext == "jpeg" || ext == "jpe" || hasMagic(sampleBytes, "\xFF\xD8\xFF"))
                return {QStringLiteral("image"), QStringLiteral("JPEG"), QStringLiteral("Immagine JPEG"), false};
            if (ext == "gif" || hasMagic(sampleBytes, "GIF87a") || hasMagic(sampleBytes, "GIF89a"))
                return {QStringLiteral("image"), QStringLiteral("GIF"), QStringLiteral("Immagine GIF"), false};
            if (ext == "webp" || (hasMagic(sampleBytes, "RIFF", 0) && hasMagic(sampleBytes, "WEBP", 8)))
                return {QStringLiteral("image"), QStringLiteral("WEBP"), QStringLiteral("Immagine WebP"), false};
            if (ext == "ico" || ext == "cur")
                return {QStringLiteral("image"), QStringLiteral("ICO"), QStringLiteral("Icona ICO"), false};
            if (ext == "bmp" || hasMagic(sampleBytes, "BM"))
                return {QStringLiteral("image"), QStringLiteral("BMP"), QStringLiteral("Immagine Bitmap"), false};
            if (ext == "tif" || ext == "tiff")
                return {QStringLiteral("image"), QStringLiteral("TIFF"), QStringLiteral("Immagine TIFF"), false};
            if (ext == "avif" || ext == "heic" || ext == "heif" || ext == "qoi" || ext == "tga" || ext == "ppm" || ext == "pgm" || ext == "pbm")
                return {QStringLiteral("image"), QString::fromStdString(ext).toUpper(), QStringLiteral("Immagine %1").arg(QString::fromStdString(ext).toUpper()), false};

            if (ext == "pdf" || hasMagic(sampleBytes, "%PDF-"))
                return {QStringLiteral("pdf"), QStringLiteral("PDF"), QStringLiteral("Documento PDF"), false};

            if (ext == "ttf" || ext == "otf" || ext == "ttc" ||
                hasMagic(sampleBytes, "OTTO") || hasMagic(sampleBytes, "ttcf"))
                return {QStringLiteral("font"), QString::fromStdString(ext.empty() ? "FONT" : ext).toUpper(), QStringLiteral("Font tipografico"), false};
            if (ext == "woff" || ext == "woff2")
                return {QStringLiteral("font"), QString::fromStdString(ext).toUpper(), QStringLiteral("Web Font %1").arg(QString::fromStdString(ext).toUpper()), false};

            if (ext == "docx")
                return {QStringLiteral("archive"), QStringLiteral("DOCX"), QStringLiteral("Documento Word (OpenXML)"), true};
            if (ext == "xlsx")
                return {QStringLiteral("archive"), QStringLiteral("XLSX"), QStringLiteral("Foglio di calcolo Excel (OpenXML)"), true};
            if (ext == "pptx")
                return {QStringLiteral("archive"), QStringLiteral("PPTX"), QStringLiteral("Presentazione PowerPoint (OpenXML)"), true};
            if (ext == "odt" || ext == "ods" || ext == "odp")
                return {QStringLiteral("archive"), QString::fromStdString(ext).toUpper(), QStringLiteral("Documento OpenDocument"), true};
            if (ext == "zip" || ext == "jar" || ext == "war" || ext == "apk" || ext == "epub" || ext == "whl" || ext == "nupkg" || ext == "xpi")
                return {QStringLiteral("archive"), QString::fromStdString(ext).toUpper(), QStringLiteral("Archivio %1").arg(QString::fromStdString(ext).toUpper()), true};
            if (hasMagic(sampleBytes, "PK\x03\x04") || hasMagic(sampleBytes, "PK\x05\x06"))
                return {QStringLiteral("archive"), ext.empty() ? QStringLiteral("ZIP") : QString::fromStdString(ext).toUpper(), QStringLiteral("Archivio ZIP"), true};

            if (ext == "tar" || ext == "gz" || ext == "tgz" || ext == "bz2" || ext == "xz" || ext == "zst" ||
                ext == "7z" || ext == "rar" || ext == "deb" || ext == "rpm")
                return {QStringLiteral("archive"), QString::fromStdString(ext).toUpper(), QStringLiteral("Archivio / Pacchetto %1").arg(QString::fromStdString(ext).toUpper()), false};

            if (ext == "wav" || (hasMagic(sampleBytes, "RIFF", 0) && hasMagic(sampleBytes, "WAVE", 8)))
                return {QStringLiteral("audio"), QStringLiteral("WAV"), QStringLiteral("Audio WAV"), false};
            if (ext == "mp3" || ext == "ogg" || ext == "flac" || ext == "m4a" || ext == "aac" || ext == "opus" || ext == "wma")
                return {QStringLiteral("audio"), QString::fromStdString(ext).toUpper(), QStringLiteral("Audio %1").arg(QString::fromStdString(ext).toUpper()), false};

            if (ext == "mp4" || ext == "webm" || ext == "mkv" || ext == "mov" || ext == "avi" || ext == "m4v" || ext == "ogv")
                return {QStringLiteral("video"), QString::fromStdString(ext).toUpper(), QStringLiteral("Video %1").arg(QString::fromStdString(ext).toUpper()), false};

            if (ext == "sqlite" || ext == "sqlite3" || ext == "db" || hasMagic(sampleBytes, "SQLite format 3"))
                return {QStringLiteral("database"), QStringLiteral("SQLITE"), QStringLiteral("Database SQLite"), false};

            if (hasMagic(sampleBytes, "\x7f" "ELF") || ext == "so" || ext == "o" || ext == "a")
                return {QStringLiteral("executable"), QStringLiteral("ELF"), QStringLiteral("Binario / Libreria ELF"), false};
            if (ext == "wasm" || hasMagic(sampleBytes, std::string_view("\0asm", 4)))
                return {QStringLiteral("executable"), QStringLiteral("WASM"), QStringLiteral("Modulo WebAssembly"), false};
            if (ext == "exe" || ext == "dll" || (ext == "sys" && hasMagic(sampleBytes, "MZ")))
                return {QStringLiteral("executable"), QString::fromStdString(ext).toUpper(), QStringLiteral("Eseguibile / Libreria Windows"), false};
            if (ext == "dylib" || ext == "class" || ext == "pyc")
                return {QStringLiteral("executable"), QString::fromStdString(ext).toUpper(), QStringLiteral("File compilato %1").arg(QString::fromStdString(ext).toUpper()), false};

            if (ext == "doc" || ext == "xls" || ext == "ppt" || ext == "rtf")
                return {QStringLiteral("document"), QString::fromStdString(ext).toUpper(), QStringLiteral("Documento %1").arg(QString::fromStdString(ext).toUpper()), false};

            return {QStringLiteral("binary"),
                    ext.empty() ? QStringLiteral("BINARY") : QString::fromStdString(ext).toUpper(),
                    QStringLiteral("File binario"),
                    false};
        }
    } // namespace

    bool DiffFileInspector::isSvgPath(const std::string& filePath)
    {
        const std::string ext = lowercaseExtension(filePath);
        return ext == "svg" || ext == "svgz";
    }

    bool DiffFileInspector::canDecodeSvg()
    {
        static const bool supported = []()
        {
            const auto formats = QImageReader::supportedImageFormats();
            for (const auto& fmt : formats)
            {
                if (fmt.compare("svg", Qt::CaseInsensitive) == 0)
                {
                    return true;
                }
            }
            return false;
        }();
        return supported;
    }

    QString DiffFileInspector::formatByteSize(qint64 bytes)
    {
        if (bytes < 0) return QStringLiteral("0 B");
        if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
        const double kb = static_cast<double>(bytes) / 1024.0;
        if (kb < 1024.0) return QStringLiteral("%1 KB").arg(kb, 0, 'f', kb < 10.0 ? 2 : 1);
        const double mb = kb / 1024.0;
        if (mb < 1024.0) return QStringLiteral("%1 MB").arg(mb, 0, 'f', mb < 10.0 ? 2 : 1);
        const double gb = mb / 1024.0;
        return QStringLiteral("%1 GB").arg(gb, 0, 'f', 2);
    }

    QString DiffFileInspector::formatSizeDelta(qint64 oldSize, qint64 newSize, bool hasOld, bool hasNew)
    {
        if (!hasOld && hasNew)
        {
            return QStringLiteral("+%1").arg(formatByteSize(newSize));
        }
        if (hasOld && !hasNew)
        {
            return QStringLiteral("-%1").arg(formatByteSize(oldSize));
        }
        if (!hasOld && !hasNew)
        {
            return {};
        }
        const qint64 diff = newSize - oldSize;
        if (diff == 0) return QStringLiteral("0 B (invariata)");
        const QString sign = diff > 0 ? QStringLiteral("+") : QStringLiteral("-");
        if (oldSize > 0)
        {
            const double pct = (100.0 * static_cast<double>(diff)) / static_cast<double>(oldSize);
            return QStringLiteral("%1%2 (%3%4%)")
                   .arg(sign, formatByteSize(std::abs(diff)), diff > 0 ? QStringLiteral("+") : QStringLiteral(""))
                   .arg(pct, 0, 'f', 1);
        }
        return sign + formatByteSize(std::abs(diff));
    }

    DiffFileInspection DiffFileInspector::inspect(
        const std::string& filePath,
        const std::optional<gf::git::bo::FileBlob>& oldBlob,
        const std::optional<gf::git::bo::FileBlob>& newBlob)
    {
        DiffFileInspection out;
        out.hasOldBlob = oldBlob.has_value();
        out.hasNewBlob = newBlob.has_value();
        if (!out.hasOldBlob && !out.hasNewBlob)
        {
            return out;
        }

        out.valid = true;
        const std::string emptyStr;
        const std::string& oldBytes = out.hasOldBlob ? oldBlob->bytes : emptyStr;
        const std::string& newBytes = out.hasNewBlob ? newBlob->bytes : emptyStr;
        out.oldFileSize = out.hasOldBlob ? static_cast<qint64>(oldBlob->totalSize) : 0;
        out.newFileSize = out.hasNewBlob ? static_cast<qint64>(newBlob->totalSize) : 0;
        out.oldTruncated = out.hasOldBlob && oldBlob->truncated;
        out.newTruncated = out.hasNewBlob && newBlob->truncated;

        if (out.hasOldBlob && !out.oldTruncated)
        {
            out.oldSha256 = computeSha256Hex(oldBytes);
        }
        if (out.hasNewBlob && !out.newTruncated)
        {
            out.newSha256 = computeSha256Hex(newBytes);
        }

        const std::string ext = lowercaseExtension(filePath);
        const std::string& sample = !newBytes.empty() ? newBytes : oldBytes;
        const auto cls = classifyFile(ext, sample);

        out.fileKind = cls.kind;
        out.formatBadge = cls.badge;
        out.formatDescription = cls.description;
        out.isSvg = (ext == "svg" || ext == "svgz");

        if (out.oldTruncated || out.newTruncated)
        {
            out.notice = QStringLiteral("File superiore a %1 — anteprima limitata ai metadati e ai primi byte.")
                         .arg(formatByteSize(static_cast<qint64>(kMaxBlobBytes)));
        }

        // 1. Try image decoding (images, SVG, or PDF page 1 if Qt PDF image plugin is available)
        const bool tryDecode = (cls.kind == QLatin1String("image") || cls.kind == QLatin1String("pdf"));
        if (tryDecode)
        {
            if (out.hasOldBlob && !out.oldTruncated)
            {
                auto dec = tryDecodeImage(oldBytes, ext);
                if (!dec.image.isNull())
                {
                    out.oldImage = std::move(dec.image);
                    out.oldImageWidth = dec.width;
                    out.oldImageHeight = dec.height;
                    out.oldImageDepth = dec.depth;
                    out.oldImageHasAlpha = dec.hasAlpha;
                }
            }
            if (out.hasNewBlob && !out.newTruncated)
            {
                auto dec = tryDecodeImage(newBytes, ext);
                if (!dec.image.isNull())
                {
                    out.newImage = std::move(dec.image);
                    out.newImageWidth = dec.width;
                    out.newImageHeight = dec.height;
                    out.newImageDepth = dec.depth;
                    out.newImageHasAlpha = dec.hasAlpha;
                }
            }

            out.hasVisualPreview = !out.oldImage.isNull() || !out.newImage.isNull();
            if (out.hasVisualPreview && cls.kind == QLatin1String("image"))
            {
                out.isImage = true;
            }
            if (!out.oldImage.isNull() && !out.newImage.isNull())
            {
                computePixelDiff(out.oldImage, out.newImage,
                                 out.diffImage, out.imageDiffPercent);
            }
        }

        // 2. General metadata rows
        appendMetaRow(out.metadataRows, QStringLiteral("Formato"),
                      out.hasOldBlob ? out.formatDescription : QString(),
                      out.hasNewBlob ? out.formatDescription : QString());
        appendMetaRow(out.metadataRows, QStringLiteral("Dimensione"),
                      out.hasOldBlob ? QStringLiteral("%1 (%2 B)").arg(formatByteSize(out.oldFileSize)).arg(out.oldFileSize) : QString(),
                      out.hasNewBlob ? QStringLiteral("%1 (%2 B)").arg(formatByteSize(out.newFileSize)).arg(out.newFileSize) : QString());

        if (out.hasVisualPreview)
        {
            appendMetaRow(out.metadataRows, QStringLiteral("Risoluzione"),
                          out.oldImageWidth > 0 ? QStringLiteral("%1 × %2 px").arg(out.oldImageWidth).arg(out.oldImageHeight) : QString(),
                          out.newImageWidth > 0 ? QStringLiteral("%1 × %2 px").arg(out.newImageWidth).arg(out.newImageHeight) : QString());
            appendMetaRow(out.metadataRows, QStringLiteral("Profondità / Alpha"),
                          out.oldImageDepth > 0
                              ? QStringLiteral("%1-bit%2").arg(out.oldImageDepth).arg(out.oldImageHasAlpha ? QStringLiteral(" (con trasparenza)") : QString())
                              : QString(),
                          out.newImageDepth > 0
                              ? QStringLiteral("%1-bit%2").arg(out.newImageDepth).arg(out.newImageHasAlpha ? QStringLiteral(" (con trasparenza)") : QString())
                              : QString());
        }

        // 3. Format-specific deep inspection
        if (cls.kind == QLatin1String("pdf"))
        {
            const PdfInfo oldPdf = out.hasOldBlob ? inspectPdf(oldBytes) : PdfInfo{};
            const PdfInfo newPdf = out.hasNewBlob ? inspectPdf(newBytes) : PdfInfo{};
            appendMetaRow(out.metadataRows, QStringLiteral("Versione PDF"), oldPdf.version, newPdf.version);
            appendMetaRow(out.metadataRows, QStringLiteral("Pagine (stima)"),
                          oldPdf.pages > 0 ? QString::number(oldPdf.pages) : QString(),
                          newPdf.pages > 0 ? QString::number(newPdf.pages) : QString());
            appendMetaRow(out.metadataRows, QStringLiteral("Titolo"), oldPdf.title, newPdf.title);
            appendMetaRow(out.metadataRows, QStringLiteral("Autore"), oldPdf.author, newPdf.author);
        }
        else if (cls.isZipBased)
        {
            const auto oldEntries = (out.hasOldBlob && !out.oldTruncated)
                                        ? parseZipCentralDirectory(oldBytes)
                                        : std::map<std::string, ZipEntryInfo>{};
            const auto newEntries = (out.hasNewBlob && !out.newTruncated)
                                        ? parseZipCentralDirectory(newBytes)
                                        : std::map<std::string, ZipEntryInfo>{};

            std::vector<std::string> allNames;
            allNames.reserve(oldEntries.size() + newEntries.size());
            for (const auto& [k, _] : oldEntries) allNames.push_back(k);
            for (const auto& [k, _] : newEntries)
            {
                if (!oldEntries.contains(k)) allNames.push_back(k);
            }
            std::sort(allNames.begin(), allNames.end());

            for (const auto& name : allNames)
            {
                const auto itOld = oldEntries.find(name);
                const auto itNew = newEntries.find(name);
                const bool inOld = itOld != oldEntries.end();
                const bool inNew = itNew != newEntries.end();

                QString status;
                if (!inOld && inNew)
                {
                    status = QStringLiteral("added");
                    ++out.archiveAddedCount;
                }
                else if (inOld && !inNew)
                {
                    status = QStringLiteral("removed");
                    ++out.archiveRemovedCount;
                }
                else if (itOld->second.crc32 != itNew->second.crc32 ||
                         itOld->second.uncompressedSize != itNew->second.uncompressedSize)
                {
                    status = QStringLiteral("modified");
                    ++out.archiveModifiedCount;
                }
                else
                {
                    status = QStringLiteral("unchanged");
                    ++out.archiveUnchangedCount;
                }

                QVariantMap e;
                e[QStringLiteral("path")] = QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
                e[QStringLiteral("status")] = status;
                e[QStringLiteral("oldSize")] = inOld ? static_cast<qint64>(itOld->second.uncompressedSize) : -1;
                e[QStringLiteral("newSize")] = inNew ? static_cast<qint64>(itNew->second.uncompressedSize) : -1;
                e[QStringLiteral("oldSizeText")] = inOld ? formatByteSize(static_cast<qint64>(itOld->second.uncompressedSize)) : QString();
                e[QStringLiteral("newSizeText")] = inNew ? formatByteSize(static_cast<qint64>(itNew->second.uncompressedSize)) : QString();
                out.archiveEntries.append(e);
            }

            appendMetaRow(out.metadataRows, QStringLiteral("File nell'archivio"),
                          out.hasOldBlob ? QString::number(oldEntries.size()) : QString(),
                          out.hasNewBlob ? QString::number(newEntries.size()) : QString());
        }
        else if (cls.kind == QLatin1String("font"))
        {
            if (ext == "ttf" || ext == "otf" || ext == "ttc")
            {
                if (out.hasOldBlob && !out.oldTruncated && !oldBytes.empty())
                {
                    out.oldFontBytes = QByteArray(oldBytes.data(), static_cast<qsizetype>(oldBytes.size()));
                }
                if (out.hasNewBlob && !out.newTruncated && !newBytes.empty())
                {
                    out.newFontBytes = QByteArray(newBytes.data(), static_cast<qsizetype>(newBytes.size()));
                }
            }
        }
        else if (cls.kind == QLatin1String("audio"))
        {
            const WavInfo oldWav = out.hasOldBlob ? inspectWav(oldBytes) : WavInfo{};
            const WavInfo newWav = out.hasNewBlob ? inspectWav(newBytes) : WavInfo{};
            if (oldWav.valid || newWav.valid)
            {
                auto fmtAudio = [](const WavInfo& w) -> QString
                {
                    if (!w.valid) return {};
                    return QStringLiteral("%1 Hz • %2 ch • %3-bit")
                           .arg(w.sampleRate)
                           .arg(w.channels)
                           .arg(w.bitsPerSample);
                };
                auto fmtDur = [](const WavInfo& w) -> QString
                {
                    if (!w.valid || w.durationSec <= 0.0) return {};
                    return QStringLiteral("%1 s").arg(w.durationSec, 0, 'f', 2);
                };
                appendMetaRow(out.metadataRows, QStringLiteral("Stream audio"), fmtAudio(oldWav), fmtAudio(newWav));
                appendMetaRow(out.metadataRows, QStringLiteral("Durata"), fmtDur(oldWav), fmtDur(newWav));
            }
        }
        else if (cls.kind == QLatin1String("database"))
        {
            appendMetaRow(out.metadataRows, QStringLiteral("Struttura SQLite"),
                          out.hasOldBlob ? inspectSqliteSummary(oldBytes) : QString(),
                          out.hasNewBlob ? inspectSqliteSummary(newBytes) : QString());
        }
        else if (cls.kind == QLatin1String("executable"))
        {
            appendMetaRow(out.metadataRows, QStringLiteral("Target ELF"),
                          out.hasOldBlob ? inspectElfSummary(oldBytes) : QString(),
                          out.hasNewBlob ? inspectElfSummary(newBytes) : QString());
        }

        if (!out.oldSha256.isEmpty() || !out.newSha256.isEmpty())
        {
            appendMetaRow(out.metadataRows, QStringLiteral("SHA-256"),
                          out.oldSha256.left(16),
                          out.newSha256.left(16));
        }

        // 4. Build Hex dump / diff preview (skip for SVG text files since they have real text diff)
        if (!out.isSvg)
        {
            buildHexRows(oldBytes, out.hasOldBlob,
                         newBytes, out.hasNewBlob,
                         out.oldFileSize, out.newFileSize,
                         out.hexRows, out.hexTruncated, out.hexChangedRows);
        }

        return out;
    }
} // namespace gf::desktop
