#include "models/RunResult.h"
#include "pyrun/version.h"

#include <QtTest>

using namespace pyrun;

class SmokeTest : public QObject {
    Q_OBJECT
private slots:
    void versionComesFromCMake() {
        QCOMPARE(QString::fromLatin1(kVersion), QStringLiteral(PYRUN_PROJECT_VERSION));
    }

    void formatResultText_data() {
        QTest::addColumn<int>("outcome");
        QTest::addColumn<int>("exitCode");
        QTest::addColumn<QString>("expectedPrefix");
        QTest::newRow("ok") << int(RunOutcome::Exited) << 0 << "[ok] exit code 0";
        QTest::newRow("nonzero") << int(RunOutcome::Exited) << 3 << "[failed] exit code 3";
        QTest::newRow("crash") << int(RunOutcome::Crashed) << -1 << "[failed] process crashed";
        QTest::newRow("stopped") << int(RunOutcome::Stopped) << -1 << "[stopped]";
        QTest::newRow("timeout") << int(RunOutcome::TimedOut) << -1 << "[timeout]";
        QTest::newRow("nostart") << int(RunOutcome::FailedToStart) << -1 << "[failed] could not run";
    }
    void formatResultText() {
        QFETCH(int, outcome);
        QFETCH(int, exitCode);
        QFETCH(QString, expectedPrefix);
        RunResult r;
        r.outcome = static_cast<RunOutcome>(outcome);
        r.exitCode = exitCode;
        QVERIFY2(pyrun::formatResult(r).startsWith(expectedPrefix), qPrintable(pyrun::formatResult(r)));
    }
};

QTEST_GUILESS_MAIN(SmokeTest)
#include "tst_smoke.moc"
