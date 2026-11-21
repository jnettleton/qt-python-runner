#include "Fakes.h"
#include "presenters/RunPresenter.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace pyrun;
using namespace pyrun::testing;
using namespace std::chrono_literals;

class RunPresenterTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    FakeView view;
    FakeRunner runner;
    FakeLocator locator;
    std::unique_ptr<RunPresenter> presenter;
    std::unique_ptr<QSignalSpy> finishedSpy;

    QString script(const QString& name) const {
        const QString path = m_dir.filePath(name);
        QFile f(path);
        if (!f.exists() && f.open(QIODevice::WriteOnly)) {
            f.write("pass\n");
        }
        return path;
    }
    BatchSummary summary() const {
        return qvariant_cast<BatchSummary>(finishedSpy->last().at(0));
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    // Fresh fakes and presenter per test.
    void init() {
        view = FakeView{};
        runner.requests.clear();
        runner.stopCalls = 0;
        locator.lookup = {QStringLiteral("C:/py/python.exe"), {}};
        presenter = std::make_unique<RunPresenter>(view, runner, locator);
        finishedSpy = std::make_unique<QSignalSpy>(presenter.get(), &RunPresenter::batchFinished);
    }
    void cleanup() {
        finishedSpy.reset();
        presenter.reset();
    }

    void outputIsForwardedWithItsChannelTag() {
        presenter->run({script("a.py")}, {});
        runner.emitOutput(QStringLiteral("to out\n"), OutputChannel::StdOut);
        runner.emitOutput(QStringLiteral("to err\n"), OutputChannel::StdErr);
        runner.emitOutput(QStringLiteral("more out\n"));  // fake defaults to stdout
        runner.complete(RunOutcome::Exited, 0);

        QCOMPARE(view.chunks.size(), size_t(3));
        QCOMPARE(view.chunks[0].first, QStringLiteral("to out\n"));
        QCOMPARE(view.chunks[0].second, OutputChannel::StdOut);
        QCOMPARE(view.chunks[1].first, QStringLiteral("to err\n"));
        QCOMPARE(view.chunks[1].second, OutputChannel::StdErr);
        QCOMPARE(view.chunks[2].second, OutputChannel::StdOut);
    }

    void stderrIsNeverRetaggedOrDroppedAcrossScripts() {
        presenter->run({script("a.py"), script("b.py")}, {});
        runner.emitOutput(QStringLiteral("e1\n"), OutputChannel::StdErr);
        runner.complete(RunOutcome::Exited, 1);
        runner.emitOutput(QStringLiteral("e2\n"), OutputChannel::StdErr);
        runner.complete(RunOutcome::Exited, 0);

        QCOMPARE(view.chunks.size(), size_t(2));
        QCOMPARE(view.chunks[0].second, OutputChannel::StdErr);
        QCOMPARE(view.chunks[1].second, OutputChannel::StdErr);
    }

    // The presenter's own line-break repair is neutral text, not stderr.
    void lineBreakRepairAfterPartialStderrIsStdout() {
        presenter->run({script("a.py")}, {});
        runner.emitOutput(QStringLiteral("no newline"), OutputChannel::StdErr);
        runner.complete(RunOutcome::Exited, 0);

        QCOMPARE(view.chunks.size(), size_t(2));
        QCOMPARE(view.chunks[1].first, QStringLiteral("\n"));
        QCOMPARE(view.chunks[1].second, OutputChannel::StdOut);
    }

    // The presenter, not the view, decides what counts as a failure.
    void resultSeverityFollowsTheOutcome_data() {
        QTest::addColumn<int>("outcome");
        QTest::addColumn<int>("exitCode");
        QTest::addColumn<int>("severity");
        QTest::addColumn<QString>("marker");
        const int ok = int(StatusSeverity::Success);
        const int bad = int(StatusSeverity::Failure);
        QTest::newRow("ok") << int(RunOutcome::Exited) << 0 << ok << "[ok]";
        QTest::newRow("non-zero exit") << int(RunOutcome::Exited) << 3 << bad << "[failed]";
        QTest::newRow("timeout") << int(RunOutcome::TimedOut) << -1 << bad << "[timeout]";
        QTest::newRow("stopped") << int(RunOutcome::Stopped) << -1 << bad << "[stopped]";
        QTest::newRow("crash") << int(RunOutcome::Crashed) << -1 << bad << "[failed]";
        QTest::newRow("failed to start") << int(RunOutcome::FailedToStart) << 0 << bad << "[failed]";
    }
    void resultSeverityFollowsTheOutcome() {
        QFETCH(int, outcome);
        QFETCH(int, exitCode);
        QFETCH(int, severity);
        QFETCH(QString, marker);
        presenter->run({script("a.py")}, {});
        runner.complete(static_cast<RunOutcome>(outcome), exitCode);

        qsizetype found = -1;
        for (qsizetype i = 0; i < view.statuses.size(); ++i) {
            if (view.statuses.at(i).startsWith(marker)) {
                found = i;
            }
        }
        QVERIFY2(found >= 0, qPrintable(view.statuses.join('|')));
        QCOMPARE(int(view.statusSeverities.at(size_t(found))), severity);
    }

    void headersAreInfoAndSummaryFollowsTheBatch() {
        presenter->run({script("a.py"), script("b.py")}, {});
        runner.complete(RunOutcome::Exited, 0);
        runner.complete(RunOutcome::Exited, 1);
        QCOMPARE(view.statuses.size(), view.statusSeverities.size());
        for (qsizetype i = 0; i < view.statuses.size(); ++i) {
            const QString& line = view.statuses.at(i);
            const StatusSeverity got = view.statusSeverities.at(size_t(i));
            if (line.contains("Running ") || line.contains("interpreter:") || line == "Summary:") {
                QCOMPARE(int(got), int(StatusSeverity::Info));
            } else if (line.startsWith("  ok ")) {
                QCOMPARE(int(got), int(StatusSeverity::Success));
            } else if (line.startsWith("  FAILED")) {
                QCOMPARE(int(got), int(StatusSeverity::Failure));
            }
        }
        // 1 of 2 failed: the totals line is a failure.
        QVERIFY(view.statuses.last().contains("1 failed"));
        QCOMPARE(int(view.statusSeverities.back()), int(StatusSeverity::Failure));
    }

    void cleanBatchTotalsAreSuccess() {
        presenter->run({script("a.py"), script("b.py")}, {});
        runner.complete(RunOutcome::Exited, 0);
        runner.complete(RunOutcome::Exited, 0);
        QVERIFY(view.statuses.last().contains("0 failed"));
        QCOMPARE(int(view.statusSeverities.back()), int(StatusSeverity::Success));
    }

    void singleScriptSuccess() {
        presenter->run({script("a.py")}, {});
        QCOMPARE(runner.requests.size(), size_t(1));
        QCOMPARE(runner.requests[0].interpreter, QStringLiteral("C:/py/python.exe"));
        QCOMPARE(QFileInfo(runner.requests[0].script).fileName(), QStringLiteral("a.py"));

        runner.emitOutput(QStringLiteral("hello\n"));
        runner.complete(RunOutcome::Exited, 0);

        QVERIFY(view.output.contains(QStringLiteral("hello\n")));
        QVERIFY(view.statuses.last().startsWith(QStringLiteral("[ok] exit code 0")));
        QCOMPARE(finishedSpy->count(), 1);
        QCOMPARE(summary().exitCode(), 0);
        QCOMPARE(view.runningHistory, (std::vector<bool>{true, false}));
        QVERIFY(!presenter->isRunning());
    }

    void passesOptionsThrough() {
        RunOptions opts;
        opts.python = QStringLiteral("D:/custom/python.exe");
        opts.timeout = 5s;
        presenter->run({script("a.py")}, opts);
        QCOMPARE(locator.lastOverride, QStringLiteral("D:/custom/python.exe"));
        QCOMPARE(runner.requests[0].timeout, 5000ms);
    }

    void continuesPastFailureByDefault() {
        presenter->run({script("a.py"), script("b.py"), script("c.py")}, {});
        runner.complete(RunOutcome::Exited, 0);
        runner.complete(RunOutcome::Exited, 3);  // b fails
        QCOMPARE(runner.requests.size(), size_t(3));  // c still started
        runner.complete(RunOutcome::Exited, 0);

        const BatchSummary s = summary();
        QCOMPARE(s.results.size(), size_t(3));
        QCOMPARE(s.failedCount(), 1);
        QCOMPARE(s.skipped, 0);
        QCOMPARE(s.exitCode(), 1);
        QVERIFY(view.statuses.contains(QStringLiteral("2 succeeded, 1 failed, 0 not run")));
    }

    void failFastStopsAtFirstFailure() {
        RunOptions opts;
        opts.failFast = true;
        presenter->run({script("a.py"), script("b.py"), script("c.py")}, opts);
        runner.complete(RunOutcome::Exited, 0);
        runner.complete(RunOutcome::Exited, 1);
        QCOMPARE(runner.requests.size(), size_t(2));
        QCOMPARE(summary().skipped, 1);
        QCOMPARE(summary().exitCode(), 1);
    }

    void failedToStartCountsAsFailure() {
        presenter->run({script("a.py")}, {});
        runner.complete(RunOutcome::FailedToStart);
        QCOMPARE(summary().exitCode(), 1);
        QVERIFY(view.statuses.last().contains(QStringLiteral("could not run")));
    }

    void timeoutAndCrashCountAsFailures_data() {
        QTest::addColumn<int>("outcome");
        QTest::newRow("timeout") << int(RunOutcome::TimedOut);
        QTest::newRow("crash") << int(RunOutcome::Crashed);
    }
    void timeoutAndCrashCountAsFailures() {
        QFETCH(int, outcome);
        presenter->run({script("a.py"), script("b.py")}, {});
        runner.complete(static_cast<RunOutcome>(outcome), -1);
        runner.complete(RunOutcome::Exited, 0);  // continue policy: b still runs
        QCOMPARE(summary().failedCount(), 1);
        QCOMPARE(summary().exitCode(), 1);
    }

    void missingScriptIsAFailureNotACrash() {
        presenter->run({m_dir.filePath("nope.py"), script("b.py")}, {});
        // nope.py rejected without touching the runner; b.py started.
        QCOMPARE(runner.requests.size(), size_t(1));
        QCOMPARE(QFileInfo(runner.requests[0].script).fileName(), QStringLiteral("b.py"));
        runner.complete(RunOutcome::Exited, 0);
        QCOMPARE(summary().exitCode(), 1);
        QVERIFY(view.output.contains(QStringLiteral("Script not found")));
    }

    void rejectsNonPyAndEmptyPaths() {
        presenter->run({script("notes.txt"), QString()}, {});
        QCOMPARE(runner.requests.size(), size_t(0));
        QCOMPARE(summary().failedCount(), 2);
        QVERIFY(view.output.contains(QStringLiteral("Not a .py file")));
        QVERIFY(view.output.contains(QStringLiteral("No script selected")));
    }

    void noInterpreterIsFatalAndRunsNothing() {
        locator.lookup = {{}, QStringLiteral("No Python interpreter found")};
        presenter->run({script("a.py")}, {});
        QCOMPARE(runner.requests.size(), size_t(0));
        QCOMPARE(view.errors, QStringList{QStringLiteral("No Python interpreter found")});
        QCOMPARE(summary().exitCode(), 2);
        QVERIFY(view.runningHistory.empty());
    }

    void emptyScriptListIsFatal() {
        presenter->run({}, {});
        QCOMPARE(summary().exitCode(), 2);
    }

    void stopKillsCurrentAndSkipsRest() {
        presenter->run({script("a.py"), script("b.py")}, {});
        presenter->stop();
        QCOMPARE(runner.stopCalls, 1);
        runner.complete(RunOutcome::Stopped, -1);
        QCOMPARE(runner.requests.size(), size_t(1));
        QCOMPARE(summary().skipped, 1);
        QCOMPARE(summary().exitCode(), 1);
        QVERIFY(view.statuses.last().contains(QStringLiteral("not run")));
    }

    void stopWhenIdleIsHarmless() {
        presenter->stop();
        QCOMPARE(runner.stopCalls, 0);
    }

    void secondRunWhileRunningIsRefused() {
        presenter->run({script("a.py")}, {});
        presenter->run({script("b.py")}, {});
        QCOMPARE(runner.requests.size(), size_t(1));
        QCOMPARE(view.errors.size(), 1);
    }

    void statusStartsOnAFreshLine() {
        presenter->run({script("a.py")}, {});
        runner.emitOutput(QStringLiteral("no newline"));
        runner.complete(RunOutcome::Exited, 0);
        QVERIFY(view.output.contains(QStringLiteral("no newline\n[ok]")));
    }

    void storeStubGetsAnActionableMessage() {
        locator.lookup = {QStringLiteral("C:/Users/x/AppData/Local/Microsoft/WindowsApps/python.exe"),
                          {}};
        presenter->run({script("a.py")}, {});
        runner.complete(RunOutcome::Exited, 9009, /*producedOutput*/ false, 20ms);
        QVERIFY(view.output.contains(QStringLiteral("Microsoft Store")));
    }

    void realInterpreterWithQuietFailureGetsNoStoreHint() {
        presenter->run({script("a.py")}, {});
        runner.complete(RunOutcome::Exited, 1, false, 20ms);
        QVERIFY(!view.output.contains(QStringLiteral("Microsoft Store")));
    }

    void clearsViewOncePerRun() {
        presenter->run({script("a.py"), script("b.py")}, {});
        runner.complete(RunOutcome::Exited, 0);
        runner.complete(RunOutcome::Exited, 0);
        QCOMPARE(view.clearCount, 1);
    }
};

QTEST_GUILESS_MAIN(RunPresenterTest)
#include "tst_runpresenter.moc"
