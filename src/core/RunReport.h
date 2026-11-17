// Scenario run report: one self-contained HTML page (inline SVG charts, no
// external assets) and the raw data as CSV, built from the report JSON the
// GUI's RunRecorder collects.
//
// Report JSON:
// { name, startedAt, endedAt (ISO 8601), durationS, completed (bool),
//   config: { mode, ports, groundName, vehicleName, preset, classes: [text] },
//   steps:   [{ index, type, preset, plannedS, startS, endS }],
//   outages: [{ startS, endS, sources: [..], dropsUp, dropsDown, recoveries: [{ socket, seconds }] }],
//   samples: [{ t, upKbps, downKbps, upDropsPerS, downDropsPerS, upDrops, downDrops }],
//   sockets: [{ t, socket, state, rtoMs, backoff, retrans, status }],
//   log:     [{ t, category, message }] }
// All t / startS / endS are seconds from the start of the run.
#pragma once

#include <QJsonObject>
#include <QString>

namespace dlsim {

QString renderReportHtml(const QJsonObject &report);
QString reportStatsCsv(const QJsonObject &report);
QString reportSocketsCsv(const QJsonObject &report);
QString reportLogCsv(const QJsonObject &report);

} // namespace dlsim
