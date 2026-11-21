// Integration tests: real QProcess, real Python, tiny fixture scripts.
// Skipped cleanly when no working interpreter is found. Set PYRUN_TEST_PYTHON
// to force one (useful where `python` is a pyenv shim or the Store stub).
#include "services/ProcessRunner.h"
#include "services/PythonLocator.h"

#include <QProcess>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <csignal>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace pyrun;
using namespace std::chrono_literals;

namespace {

struct Run {
    QString text;  // everything, in drain order
    QString out;   // stdout only
    QString err;   // stderr only
    std::vector<std::pair<QString, OutputChannel>> chunks;
    RunResult result;
};

QString fixture(const QString& name) {
    return QDir(QStringLiteral(PYRUN_FIXTURE_DIR)).absoluteFilePath(name);
}

#ifdef Q_OS_WIN
// True while a process with this pid exists and has not exited.
bool isAlive(qint64 pid) {
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (h == nullptr) {
        return false;  // gone (or inaccessible, which for our own child means gone)
    }
    const bool alive = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return alive;
}
#else
// True while the pid exists and is not a zombie (a killed orphan stays a
// zombie until init reaps it; that counts as dead).
bool isAlive(qint64 pid) {
    if (::kill(static_cast<pid_t>(pid), 0) != 0) {
        return false;
    }
    QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!stat.open(QIODevice::ReadOnly)) {
        return true;  // no /proc (not Linux): kill(pid, 0) succeeded
    }
    const QByteArray line = stat.readAll();
    const qsizetype closeParen = line.lastIndexOf(')');
    return !(closeParen >= 0 && line.size() > closeParen + 2 && line.at(closeParen + 2) == 'Z');
}
#endif

}  // namespace

class ProcessRunnerTest : public QObject {
    Q_OBJECT

    QString m_python;

    Run runScript(const QString& script, std::chrono::milliseconds timeout = 0ms,
                  std::chrono::milliseconds stopAfter = 0ms, const QString& interpreter = {}) {
        ProcessRunner runner;  // owner: this stack frame
        Run run;
        QObject::connect(&runner, &IProcessRunner::outputReceived, &runner,
                         [&run](const QString& t, OutputChannel channel) {
                             run.text += t;
                             (channel == OutputChannel::StdErr ? run.err : run.out) += t;
                             run.chunks.emplace_back(t, channel);
                         });
        QSignalSpy spy(&runner, &IProcessRunner::finished);

        RunRequest req;
        req.interpreter = interpreter.isEmpty() ? m_python : interpreter;
        req.script = script;
        req.timeout = timeout;
        runner.start(req);
        if (stopAfter.count() > 0) {
            QTimer::singleShot(stopAfter, &runner, [&runner] { runner.stop(); });
        }
        if (spy.isEmpty()) {
            spy.wait(60000);
        }
        if (spy.count() != 1) {
            QTest::qFail("expected exactly one finished()", __FILE__, __LINE__);
            return run;
        }
        run.result = qvariant_cast<RunResult>(spy.first().at(0));
        return run;
    }

    // The WindowsApps python.exe aliases (Store, Python Install Manager) are
    // launchers: the real interpreter is a process the Job Object did not
    // capture (observed: the grandchild survives stop/timeout through them,
    // even with the child created suspended). Tree-kill is therefore only
    // asserted for a real interpreter (set PYRUN_TEST_PYTHON). Open item.
    [[nodiscard]] bool interpreterIsAlias() const {
        return PythonLocator::isStoreAlias(m_python);
    }

    void verifyGrandchildDies(const QString& text) {
        const QRegularExpressionMatch m =
            QRegularExpression(QStringLiteral("grandchild (\\d+)")).match(text);
        QVERIFY2(m.hasMatch(), qPrintable(text));
        const qint64 pid = m.captured(1).toLongLong();
        // The kill is synchronous, but give the OS a moment to tear down.
        QTRY_VERIFY2_WITH_TIMEOUT(!isAlive(pid), "grandchild survived the kill", 5000);
    }

private slots:
    void initTestCase() {
        QString candidate = qEnvironmentVariable("PYRUN_TEST_PYTHON");
        if (candidate.isEmpty()) {
            candidate = PythonLocator().locate({}).path;
        }
        if (candidate.isEmpty()) {
            QSKIP("No Python interpreter found (set PYRUN_TEST_PYTHON to force one).");
        }
        // The Store stub fails here, so this also screens it out.
        if (QProcess::execute(candidate, {QStringLiteral("-c"), QStringLiteral("pass")}) != 0) {
            QSKIP("Interpreter found but cannot run a trivial script (Store stub?).");
        }
        m_python = candidate;
    }

    void unicodeOutputRoundTrips() {
        const Run r = runScript(fixture("hello.py"));
        QCOMPARE(r.result.outcome, RunOutcome::Exited);
        QCOMPARE(r.result.exitCode, 0);
        QCOMPARE(r.text, QStringLiteral("Grüße – 日本語 – ✓\n"));
        QVERIFY(r.result.producedOutput);
        QVERIFY(r.result.succeeded());
    }

    // Separate pipes: order is exact within a channel, approximate across them,
    // so only the per-channel streams are asserted (never the interleaving).
    void stdoutAndStderrAreTaggedPerChannel() {
        const Run r = runScript(fixture("interleave.py"));
        QCOMPARE(r.out, QStringLiteral("out1\nout2\nout3\n"));
        QCOMPARE(r.err, QStringLiteral("err1\nerr2\nerr3\n"));
    }

    void stderrLineArrivesTaggedAsStderr() {
        const Run r = runScript(fixture("stderr_only.py"));
        QCOMPARE(r.result.outcome, RunOutcome::Exited);
        QCOMPARE(r.result.exitCode, 1);
        QVERIFY(r.result.producedOutput);  // stderr-only output still counts
        QVERIFY(r.out.isEmpty());
        QCOMPARE(r.err, QStringLiteral("boom: Grüße – ✓\n"));
        QVERIFY(!r.chunks.empty());
        for (const auto& chunk : r.chunks) {
            QCOMPARE(chunk.second, OutputChannel::StdErr);
        }
    }

    // Each channel owns its decoder: a multi-byte character split across two
    // reads on one pipe must survive another pipe's bytes arriving in between.
    void splitUtf8OnBothChannelsDecodesIndependently() {
        const Run r = runScript(fixture("split_utf8.py"));
        QCOMPARE(r.result.exitCode, 0);
        QCOMPARE(r.out, QStringLiteral("ü\n"));
        QCOMPARE(r.err, QStringLiteral("日\n"));
        QVERIFY(!r.text.contains(QChar(0xFFFD)));
    }

    void nonZeroExit() {
        const Run r = runScript(fixture("exit3.py"));
        QCOMPARE(r.result.outcome, RunOutcome::Exited);
        QCOMPARE(r.result.exitCode, 3);
        QVERIFY(!r.result.succeeded());
        QCOMPARE(r.text, QStringLiteral("about to fail\n"));
    }

    void outputWithoutTrailingNewlineIsDelivered() {
        const Run r = runScript(fixture("noeol.py"));
        QCOMPARE(r.text, QStringLiteral("no trailing newline"));
    }

    void largeOutputBurstIsCompleteAndIntact() {
        const Run r = runScript(fixture("big.py"));
        QCOMPARE(r.result.exitCode, 0);
        QCOMPARE(r.text.count(QLatin1Char('\n')), 200000);
        QVERIFY(r.text.startsWith(QStringLiteral("日本語 line 0\n")));
        QVERIFY(r.text.endsWith(QStringLiteral("日本語 line 199999\n")));
        QVERIFY(!r.text.contains(QChar(0xFFFD)));  // no mojibake from split sequences
    }

    void pathWithSpacesAndUnicode() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString target = dir.filePath("sp ace Grüße/スクリプト.py");
        QVERIFY(QDir().mkpath(QFileInfo(target).absolutePath()));
        QVERIFY(QFile::copy(fixture("hello.py"), target));
        const Run r = runScript(target);
        QCOMPARE(r.result.exitCode, 0);
        QCOMPARE(r.text, QStringLiteral("Grüße – 日本語 – ✓\n"));
    }

    void timeoutKillsAHangingScript() {
        const Run r = runScript(fixture("hang.py"), 500ms);
        QCOMPARE(r.result.outcome, RunOutcome::TimedOut);
        QVERIFY(r.text.startsWith(QStringLiteral("started")));  // streamed before the kill
        QVERIFY(r.result.elapsed >= 400ms);
        QVERIFY(r.result.elapsed < 10s);
    }

    void stopKillsAHangingScript() {
        const Run r = runScript(fixture("hang.py"), 0ms, 700ms);
        QCOMPARE(r.result.outcome, RunOutcome::Stopped);
        QVERIFY(!r.result.succeeded());
    }

    // Stop and timeout must kill the whole tree, not just the interpreter
    // (Windows: Job Object; POSIX: process group, SIGTERM then SIGKILL).
    void stopKillsGrandchildren() {
        if (interpreterIsAlias()) {
            QSKIP("Interpreter is a WindowsApps launcher alias; set PYRUN_TEST_PYTHON.");
        }
        const Run r = runScript(fixture("spawn_child.py"), 0ms, 2000ms);
        QCOMPARE(r.result.outcome, RunOutcome::Stopped);
        verifyGrandchildDies(r.text);
    }

    void timeoutKillsGrandchildren() {
        if (interpreterIsAlias()) {
            QSKIP("Interpreter is a WindowsApps launcher alias; set PYRUN_TEST_PYTHON.");
        }
        const Run r = runScript(fixture("spawn_child.py"), 2000ms);
        QCOMPARE(r.result.outcome, RunOutcome::TimedOut);
        verifyGrandchildDies(r.text);
    }

    // POSIX: a tree that ignores SIGTERM is SIGKILLed after the grace period,
    // grandchild included.
    void sigtermIgnoredEscalatesToSigkill() {
#ifdef Q_OS_WIN
        QSKIP("POSIX signals only; the Job Object kill has no graceful stage.");
#else
        const Run r = runScript(fixture("ignore_sigterm.py"), 0ms, 1500ms);
        QCOMPARE(r.result.outcome, RunOutcome::Stopped);
        QVERIFY2(r.result.elapsed >= 1500ms + ProcessRunner::kKillGrace - 200ms,
                 "the group must have been given the grace period before SIGKILL");
        verifyGrandchildDies(r.text);
#endif
    }

    void unstartableInterpreterReportsFailedToStart() {
        const Run r = runScript(fixture("hello.py"), 0ms, 0ms,
                                QDir::temp().filePath("no-such-python-123.exe"));
        QCOMPARE(r.result.outcome, RunOutcome::FailedToStart);
        QVERIFY(r.result.message.contains("cannot start"));
    }

    void outputIsStreamedNotHeldUntilExit() {
        ProcessRunner runner;
        QElapsedTimer clock;
        qint64 firstChunkMs = -1;
        QObject::connect(&runner, &IProcessRunner::outputReceived, &runner, [&](const QString&, OutputChannel) {
            if (firstChunkMs < 0) {
                firstChunkMs = clock.elapsed();
            }
        });
        QSignalSpy spy(&runner, &IProcessRunner::finished);
        RunRequest req;
        req.interpreter = m_python;
        req.script = fixture("hang.py");
        req.timeout = 1500ms;
        clock.start();
        runner.start(req);
        QVERIFY(spy.wait(30000));
        QVERIFY2(firstChunkMs >= 0 && firstChunkMs < 1200,
                 "first output must arrive well before the timeout kill");
    }
};

QTEST_GUILESS_MAIN(ProcessRunnerTest)
#include "tst_processrunner.moc"
