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
#include "tst_presetstore.moc"
