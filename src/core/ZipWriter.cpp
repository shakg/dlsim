#include "ZipWriter.h"

#include <QDataStream>
#include <QIODevice>

namespace dlsim {

quint32 crc32(const QByteArray &data)
{
    static quint32 table[256];
    static bool init = false;
    if (!init) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    quint32 crc = 0xFFFFFFFFu;
    for (const char ch : data)
        crc = table[(crc ^ quint8(ch)) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

QByteArray makeZip(const QList<QPair<QString, QByteArray>> &files)
{
    QByteArray out, central;
    QDataStream o(&out, QIODevice::WriteOnly), c(&central, QIODevice::WriteOnly);
    o.setByteOrder(QDataStream::LittleEndian);
    c.setByteOrder(QDataStream::LittleEndian);
    const quint16 dosTime = 0, dosDate = (1 << 5) | 1; // 1980-01-01
    for (const auto &[name, data] : files) {
        const QByteArray n = name.toUtf8();
        const quint32 crc = crc32(data), size = quint32(data.size()), offset = quint32(out.size());
        // Local file header
        o << quint32(0x04034b50) << quint16(20) << quint16(0x0800) /* UTF-8 names */ << quint16(0) /* stored */
          << dosTime << dosDate << crc << size << size << quint16(n.size()) << quint16(0);
        o.writeRawData(n.constData(), int(n.size()));
        o.writeRawData(data.constData(), int(data.size()));
        // Central directory entry
        c << quint32(0x02014b50) << quint16(20) << quint16(20) << quint16(0x0800) << quint16(0) << dosTime << dosDate
          << crc << size << size << quint16(n.size()) << quint16(0) << quint16(0) << quint16(0) << quint16(0)
          << quint32(0) << offset;
        c.writeRawData(n.constData(), int(n.size()));
    }
    const quint32 cdOffset = quint32(out.size());
    o.writeRawData(central.constData(), int(central.size()));
    o << quint32(0x06054b50) << quint16(0) << quint16(0) << quint16(files.size()) << quint16(files.size())
      << quint32(central.size()) << cdOffset << quint16(0);
    return out;
}

} // namespace dlsim
