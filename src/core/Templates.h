// Goal-based scenario templates: JSON data, expanded with the user's values.
//
// A template has "params" ([{key, label, type: number|preset|list, default, unit}])
// and "steps". In steps (and in "presets", see below) a string "$key" is
// replaced by the value, "…$key…" inside a longer string by its text, and
// {"forEach": "<list key>", "as": "<name>", "steps": [...]} repeats its steps
// once per list item, with "$<name>" bound to the item.
// "presets" (optional) describes presets the scenario needs that don't exist
// yet: [{name, from: <preset>, description, uplink: {...}, downlink: {...}}],
// fields overriding those of "from".
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QVariantMap>

namespace dlsim {

// values: key -> value; missing keys take the param's default.
// Returns {name, loop, steps, presets}.
QJsonObject expandTemplate(const QJsonObject &tpl, const QVariantMap &values);
double totalDurationS(const QJsonArray &steps);

} // namespace dlsim
