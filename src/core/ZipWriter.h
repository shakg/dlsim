// Minimal .zip writer (stored, no compression) for exporting a few CSV files
// together. Qt has no public zip API.
#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>

namespace dlsim {

quint32 crc32(const QByteArray &data);
// files: (name inside the archive, content)
QByteArray makeZip(const QList<QPair<QString, QByteArray>> &files);

} // namespace dlsim
