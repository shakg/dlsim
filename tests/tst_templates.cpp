// Unit tests for scenario template expansion and the built-in templates.
#include "Templates.h"

#include <QFile>
#include <QJsonDocument>
#include <QtTest>

using namespace dlsim;

class TestTemplates : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultsAndForEach();
    void valuesOverrideDefaults();
    void stringInterpolationAndPresets();
    void builtinsExpand();
};

static QJsonObject tpl(const char *json) { return QJsonDocument::fromJson(json).object(); }

void TestTemplates::defaultsAndForEach()
{
    const QJsonObject t = tpl(R"({"name":"R","params":[
        {"key":"base","type":"preset","default":"Good LOS"},
        {"key":"outages","type":"list","default":[3,10]},
        {"key":"recovery","type":"number","default":60}],
      "steps":[{"type":"preset","preset":"$base","durationS":"$recovery"},
               {"forEach":"outages","as":"len","steps":[{"type":"outage","durationS":"$len"},
                                                        {"type":"preset","preset":"$base","durationS":"$recovery"}]}]})");
    const QJsonObject s = expandTemplate(t, {});
    const QJsonArray steps = s.value("steps").toArray();
    QCOMPARE(steps.size(), 5);
    QCOMPARE(steps[0].toObject().value("preset").toString(), QStringLiteral("Good LOS"));
    QCOMPARE(steps[1].toObject().value("durationS").toDouble(), 3.0);
    QCOMPARE(steps[3].toObject().value("durationS").toDouble(), 10.0);
    QCOMPARE(totalDurationS(steps), 60.0 * 3 + 13);
}

void TestTemplates::valuesOverrideDefaults()
{
    const QJsonObject t = tpl(R"({"name":"R","params":[{"key":"d","type":"number","default":5}],
                                  "steps":[{"type":"outage","durationS":"$d"}]})");
    QCOMPARE(expandTemplate(t, {{"d", "12"}}).value("steps").toArray()[0].toObject().value("durationS").toDouble(), 12.0);
    QCOMPARE(expandTemplate(t, {{"d", 7}}).value("steps").toArray()[0].toObject().value("durationS").toDouble(), 7.0);
}

void TestTemplates::stringInterpolationAndPresets()
{
    const QJsonObject t = tpl(R"({"name":"C","scenarioName":"Cap $cap","params":[
        {"key":"base","type":"preset","default":"Good LOS"},{"key":"cap","type":"number","default":2000}],
      "presets":[{"name":"$base, $cap","from":"$base","downlink":{"rateKbit":"$cap"}}],
      "steps":[{"type":"preset","preset":"$base, $cap","durationS":10}]})");
    const QJsonObject s = expandTemplate(t, {{"cap", 512}});
    QCOMPARE(s.value("name").toString(), QStringLiteral("Cap 512"));
    const QJsonObject p = s.value("presets").toArray()[0].toObject();
    QCOMPARE(p.value("name").toString(), QStringLiteral("Good LOS, 512"));
    QCOMPARE(p.value("from").toString(), QStringLiteral("Good LOS"));
    QCOMPARE(p.value("downlink").toObject().value("rateKbit").toDouble(), 512.0);
    QCOMPARE(s.value("steps").toArray()[0].toObject().value("preset").toString(), QStringLiteral("Good LOS, 512"));
}

void TestTemplates::builtinsExpand()
{
    QFile f(QStringLiteral(DLSIM_TEMPLATES_JSON));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QJsonArray all = QJsonDocument::fromJson(f.readAll()).array();
    QVERIFY(all.size() >= 5);
    for (const auto &v : all) {
        const QJsonObject s = expandTemplate(v.toObject(), {});
        QVERIFY2(!s.value("name").toString().isEmpty(), qPrintable(v.toObject().value("name").toString()));
        for (const auto &st : s.value("steps").toArray())
            QVERIFY2(st.toObject().value("durationS").toDouble() > 0, qPrintable(s.value("name").toString()));
    }
}

QTEST_GUILESS_MAIN(TestTemplates)
#include "tst_templates.moc"
