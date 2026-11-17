// Unit tests for PresetStore error reporting and replace detection.
#include "PresetStore.h"

#include <QDir>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

class TestPresetStore : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void errorClearedByNextOperation();
    void presetAndScenarioErrorsAreSeparate();
    void clearErrors();
    void userPresetExists();
    void userScenarioExists();
    void missingPresets();
    void renamePreset();
    void scenarioStepTypes();
    void templates();

private:
    static QVariantMap scenario(const QString &name)
    {
        return {{"name", name}, {"loop", false},
                {"steps", QVariantList{QVariantMap{{"type", "outage"}, {"durationS", 5}}}}};
    }
};

void TestPresetStore::init()
{
    QStandardPaths::setTestModeEnabled(true);
    QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/dlsim").removeRecursively();
}

void TestPresetStore::errorClearedByNextOperation()
{
    PresetStore s;
    QVERIFY(!s.savePreset("", "", {}, {}));
    QVERIFY(!s.presetError().isEmpty());
    QVERIFY(s.savePreset("Mine", "", {}, {}));
    QVERIFY(s.presetError().isEmpty());

    QVERIFY(!s.saveScenario(scenario("Outage ladder")));
    QVERIFY(!s.scenarioError().isEmpty());
    QVERIFY(s.saveScenario(scenario("My test")));
    QVERIFY(s.scenarioError().isEmpty());
}

void TestPresetStore::presetAndScenarioErrorsAreSeparate()
{
    PresetStore s;
    QSignalSpy presetSpy(&s, &PresetStore::presetErrorChanged);
    QVERIFY(!s.saveScenario(scenario("")));
    QVERIFY(!s.scenarioError().isEmpty());
    QVERIFY(s.presetError().isEmpty());
    QCOMPARE(presetSpy.count(), 0);

    QVERIFY(!s.deletePreset("Good LOS"));
    QVERIFY(!s.presetError().isEmpty());
    QVERIFY(!s.scenarioError().isEmpty());
}

void TestPresetStore::clearErrors()
{
    PresetStore s;
    QVERIFY(!s.deletePreset("Good LOS"));
    QVERIFY(!s.deleteScenario("Outage ladder"));
    s.clearPresetError();
    QVERIFY(s.presetError().isEmpty());
    QVERIFY(!s.scenarioError().isEmpty());
    s.clearScenarioError();
    QVERIFY(s.scenarioError().isEmpty());
}

void TestPresetStore::userPresetExists()
{
    PresetStore s;
    QVERIFY(!s.userPresetExists("Mine"));
    QVERIFY(!s.userPresetExists("Good LOS")); // built-in, not a user file
    QVERIFY(!s.userPresetExists("  "));
    QVERIFY(s.savePreset("Mine", "", {}, {}));
    QVERIFY(s.userPresetExists("Mine"));
    QVERIFY(s.userPresetExists(" Mine "));
    QVERIFY(s.deletePreset("Mine"));
    QVERIFY(!s.userPresetExists("Mine"));
}

void TestPresetStore::userScenarioExists()
{
    PresetStore s;
    QVERIFY(!s.userScenarioExists("My test"));
    QVERIFY(s.saveScenario(scenario("My test")));
    QVERIFY(s.userScenarioExists("My test"));
}

QTEST_GUILESS_MAIN(TestPresetStore)
void TestPresetStore::missingPresets()
{
    PresetStore s;
    QVERIFY(s.savePreset("Mine", "", {}, {}));
    const QVariantList steps{
        QVariantMap{{"type", "preset"}, {"preset", "Mine"}, {"durationS", 5}},
        QVariantMap{{"type", "fade"}, {"preset", "Gone"}, {"durationS", 5}},
        QVariantMap{{"type", "outage"}, {"preset", "Ignored"}, {"durationS", 5}},
        QVariantMap{{"type", "preset"}, {"preset", "Gone"}, {"durationS", 5}},
        QVariantMap{{"type", "preset"}, {"preset", s.presetNames().first()}, {"durationS", 5}},
        QVariantMap{{"type", "preset"}, {"preset", "Also gone"}, {"durationS", 5}},
    };
    QCOMPARE(s.missingPresets({{"steps", steps}}), (QStringList{"Gone", "Also gone"}));
    QVERIFY(s.deletePreset("Mine"));
    QCOMPARE(s.missingPresets({{"steps", steps}}), (QStringList{"Mine", "Gone", "Also gone"}));
}

void TestPresetStore::renamePreset()
{
    PresetStore s;
    QVERIFY(s.savePreset("Mine", "desc", {{"delayMs", 40}}, {}));
    QVERIFY(s.savePreset("Other", "", {}, {}));
    QVERIFY(!s.renamePreset("Mine", "Other"));            // taken
    QVERIFY(!s.renamePreset("Mine", s.presetNames().first())); // built-in name
    QVERIFY(!s.renamePreset(s.presetNames().first(), "X")); // built-in source
    QVERIFY(s.renamePreset("Mine", "Renamed"));
    QVERIFY(s.presetError().isEmpty());
    QVERIFY(!s.presetNames().contains("Mine"));
    QCOMPARE(s.preset("Renamed").value("description").toString(), QStringLiteral("desc"));
    QCOMPARE(s.preset("Renamed").value("uplink").toMap().value("delayMs").toDouble(), 40.0);
    QVERIFY(!s.userPresetExists("Mine"));
}

void TestPresetStore::scenarioStepTypes()
{
    PresetStore s;
    const QVariantList steps{
        QVariantMap{{"type", "preset"}, {"preset", "Good LOS"}, {"durationS", 5}},
        QVariantMap{{"type", "fade"}, {"preset", "Fringe"}, {"durationS", 5}},
        QVariantMap{{"type", "outage"}, {"durationS", 5}},
        QVariantMap{{"type", "dropouts"}, {"durationS", 60}, {"meanIntervalS", 8}, {"meanDurationS", 2}, {"seed", 99}},
        QVariantMap{{"type", "dropouts"}, {"durationS", 60}}, // defaults
        QVariantMap{{"type", "bogus"}, {"durationS", 5}},
    };
    QVERIFY(s.saveScenario({{"name", "Types"}, {"steps", steps}}));
    const QVariantList got = s.scenario("Types").value("steps").toList();
    QCOMPARE(got.size(), 5); // unknown type dropped, old ones kept
    const QVariantMap d = got[3].toMap();
    QCOMPARE(d.value("type").toString(), QStringLiteral("dropouts"));
    QCOMPARE(d.value("meanIntervalS").toDouble(), 8.0);
    QCOMPARE(d.value("meanDurationS").toDouble(), 2.0);
    QCOMPARE(d.value("seed").toDouble(), 99.0);
    QCOMPARE(got[4].toMap().value("meanIntervalS").toDouble(), 30.0);
    QVERIFY(!got[4].toMap().contains("seed"));
    QVERIFY(s.missingPresets({{"steps", got}}).isEmpty()); // dropouts need no preset
}

void TestPresetStore::templates()
{
    PresetStore s;
    const QVariantList all = s.templates();
    int congested = -1;
    for (int i = 0; i < all.size(); ++i) {
        const QVariantMap e = s.expandTemplate(i, {});
        QVERIFY2(e.value("missingPresets").toStringList().isEmpty(), qPrintable(all[i].toMap().value("name").toString()));
        if (all[i].toMap().value("name").toString() == "Congested downlink")
            congested = i;
    }
    QVERIFY(congested >= 0);
    const QVariantMap e = s.expandTemplate(congested, {{"cap", 1500}});
    QCOMPARE(e.value("name").toString(), QStringLiteral("Congested downlink 1500 kbit/s"));
    const QVariantList presets = e.value("presets").toList();
    QCOMPARE(presets.size(), 1);
    const QVariantMap p = presets[0].toMap();
    QCOMPARE(p.value("name").toString(), QStringLiteral("Good LOS, downlink 1500 kbit/s"));
    QCOMPARE(p.value("downlink").toMap().value("rateKbit").toDouble(), 1500.0);
    QCOMPARE(p.value("uplink").toMap().value("delayMs").toDouble(), 8.0); // from Good LOS
    QCOMPARE(e.value("totalS").toDouble(), 180.0);
    QVERIFY(s.createTemplatePresets(e));
    QVERIFY(s.userPresetExists("Good LOS, downlink 1500 kbit/s"));
    QVERIFY(s.expandTemplate(congested, {{"cap", 1500}}).value("presets").toList().isEmpty()); // exists now
}

#include "tst_presetstore.moc"
