#include "Templates.h"

#include <QRegularExpression>

namespace dlsim {

namespace {

QString text(const QJsonValue &v)
{
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 10);
    if (v.isBool())
        return v.toBool() ? "true" : "false";
    return v.toString();
}

QJsonValue substitute(const QJsonValue &v, const QJsonObject &vars)
{
    if (v.isString()) {
        const QString s = v.toString();
        static const QRegularExpression whole(QStringLiteral("^\\$([A-Za-z_][A-Za-z0-9_]*)$"));
        if (const auto m = whole.match(s); m.hasMatch() && vars.contains(m.captured(1)))
            return vars.value(m.captured(1));
        static const QRegularExpression part(QStringLiteral("\\$([A-Za-z_][A-Za-z0-9_]*)"));
        QString out;
        qsizetype last = 0;
        for (auto it = part.globalMatch(s); it.hasNext();) {
            const auto m = it.next();
            out += s.mid(last, m.capturedStart() - last);
            out += vars.contains(m.captured(1)) ? text(vars.value(m.captured(1))) : m.captured(0);
            last = m.capturedEnd();
        }
        return out + s.mid(last);
    }
    if (v.isArray()) {
        QJsonArray a;
        for (const auto &e : v.toArray())
            a.append(substitute(e, vars));
        return a;
    }
    if (v.isObject()) {
        QJsonObject o;
        const QJsonObject in = v.toObject();
        for (auto it = in.begin(); it != in.end(); ++it)
            o.insert(it.key(), substitute(it.value(), vars));
        return o;
    }
    return v;
}

void expandSteps(const QJsonArray &in, const QJsonObject &vars, QJsonArray &out)
{
    for (const auto &v : in) {
        const QJsonObject s = v.toObject();
        if (s.contains("forEach")) {
            const QString as = s.value("as").toString(QStringLiteral("item"));
            for (const auto &item : vars.value(s.value("forEach").toString()).toArray()) {
                QJsonObject inner = vars;
                inner.insert(as, item);
                expandSteps(s.value("steps").toArray(), inner, out);
            }
            continue;
        }
        QJsonObject step = substitute(s, vars).toObject();
        // Numbers typed as text in a form still count as numbers.
        for (const char *k : {"durationS", "meanIntervalS", "meanDurationS", "seed"})
            if (step.value(k).isString())
                step.insert(k, step.value(k).toString().toDouble());
        out.append(step);
    }
}

} // namespace

QJsonObject expandTemplate(const QJsonObject &tpl, const QVariantMap &values)
{
    QJsonObject vars;
    for (const auto &p : tpl.value("params").toArray()) {
        const QJsonObject param = p.toObject();
        const QString key = param.value("key").toString();
        QJsonValue v = values.contains(key) ? QJsonValue::fromVariant(values.value(key)) : param.value("default");
        if (param.value("type").toString() == "number" && v.isString())
            v = v.toString().toDouble();
        vars.insert(key, v);
    }
    QJsonArray steps;
    expandSteps(tpl.value("steps").toArray(), vars, steps);
    return QJsonObject{
        {"name", substitute(tpl.value("scenarioName").isUndefined() ? tpl.value("name") : tpl.value("scenarioName"), vars)},
        {"loop", substitute(tpl.value("loop"), vars).toBool(false)},
        {"steps", steps},
        {"presets", substitute(tpl.value("presets").toArray(), vars)},
    };
}

double totalDurationS(const QJsonArray &steps)
{
    double t = 0;
    for (const auto &s : steps)
        t += s.toObject().value("durationS").toDouble();
    return t;
}

} // namespace dlsim
