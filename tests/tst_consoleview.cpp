// The CLI's view and colour policy, with the output captured in a file so the
// exact bytes can be checked. No console needed.
#include "ConsoleColor.h"
#include "views/ConsoleView.h"

#include <QTemporaryDir>
#include <QtTest>

#include <cstdio>
#include <memory>

using namespace pyrun;

namespace {

// Captures what a FILE* receives. Owner: the test function's stack.
class Capture {
public:
    Capture() {
        Q_ASSERT(m_dir.isValid());
        m_path = m_dir.filePath(QStringLiteral("capture.bin"));
#ifdef Q_OS_WIN
        if (fopen_s(&m_stream, QFile::encodeName(m_path).constData(), "wb") != 0) {
            m_stream = nullptr;
        }
#else
        m_stream = std::fopen(QFile::encodeName(m_path).constData(), "wb");
#endif
    }
    ~Capture() {
        if (m_stream != nullptr) {
            std::fclose(m_stream);
        }
    }
    std::FILE* stream() const { return m_stream; }
    QByteArray bytes() {
        if (m_stream != nullptr) {  // CRT fopen_s denies shared writers: close first
            std::fclose(m_stream);
            m_stream = nullptr;
        }
        QFile f(m_path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
    std::FILE* m_stream = nullptr;
};

}  // namespace

class ConsoleViewTest : public QObject {
    Q_OBJECT
private slots:
    void plainWhenNotColouring() {
        Capture out;
        Capture err;
        ConsoleView view(false, false, out.stream(), err.stream());
        view.showOutput("o\n", OutputChannel::StdOut);
        view.showOutput("e\n", OutputChannel::StdErr);
        QCOMPARE(out.bytes(), QByteArray("o\ne\n"));  // no escape bytes at all
    }

    void stderrIsWrappedWhenColouring() {
        Capture out;
        Capture err;
        ConsoleView view(true, false, out.stream(), err.stream());
        view.showOutput("o\n", OutputChannel::StdOut);
        view.showOutput("Grüße\n", OutputChannel::StdErr);
        view.showOutput("o2\n", OutputChannel::StdOut);
        QCOMPARE(out.bytes(), QByteArray("o\n\x1b[91mGr\xc3\xbc\xc3\x9f" "e\n\x1b[0mo2\n"));
    }

    void statusAndErrorsGoToStderrNotStdout() {
        Capture out;
        Capture err;
        ConsoleView view(true, true, out.stream(), err.stream());
        view.showStatus("[ok] exit code 0", StatusSeverity::Success);
        view.showError("nope");
        QVERIFY(out.bytes().isEmpty());
        QVERIFY(!err.bytes().isEmpty());
    }

    // stderr colour is independent of stdout colour: stdout may be redirected
    // (plain) while stderr is still the console (red failures), and vice versa.
    void failureStatusIsRedOnlyWhenStderrColours() {
        Capture out;
        Capture err;
        ConsoleView view(false, true, out.stream(), err.stream());
        view.showStatus("[failed] exit code 1 (0.05 s)", StatusSeverity::Failure);
        view.showStatus("[ok] exit code 0 (0.05 s)", StatusSeverity::Success);
        view.showStatus("Running x.py", StatusSeverity::Info);
        view.showError("no interpreter");
        QCOMPARE(err.bytes(),
                 QByteArray("\x1b[91m[failed] exit code 1 (0.05 s)\n\x1b[0m"
                            "[ok] exit code 0 (0.05 s)\n"
                            "Running x.py\n"
                            "\x1b[91mpyrun: error: no interpreter\n\x1b[0m"));
    }

    void failureStatusIsPlainWhenStderrDoesNotColour() {
        Capture out;
        Capture err;
        ConsoleView view(true, false, out.stream(), err.stream());  // stdout colour is irrelevant
        view.showStatus("[failed] exit code 1 (0.05 s)", StatusSeverity::Failure);
        view.showError("no interpreter");
        QCOMPARE(err.bytes(),
                 QByteArray("[failed] exit code 1 (0.05 s)\npyrun: error: no interpreter\n"));
    }

    void colourPolicy_data() {
        QTest::addColumn<bool>("flag");
        QTest::addColumn<bool>("env");
        QTest::addColumn<bool>("interactive");
        QTest::addColumn<bool>("vt");
        QTest::addColumn<bool>("expected");
        QTest::newRow("console, vt")           << false << false << true  << true  << true;
        QTest::newRow("redirected")            << false << false << false << true  << false;
        QTest::newRow("no vt support")         << false << false << true  << false << false;
        QTest::newRow("--no-color")            << true  << false << true  << true  << false;
        QTest::newRow("NO_COLOR")              << false << true  << true  << true  << false;
        QTest::newRow("both opt-outs")         << true  << true  << true  << true  << false;
    }
    void colourPolicy() {
        QFETCH(bool, flag);
        QFETCH(bool, env);
        QFETCH(bool, interactive);
        QFETCH(bool, vt);
        QFETCH(bool, expected);
        ColorPolicyInputs in;
        in.noColorFlag = flag;
        in.noColorEnv = env;
        in.interactive = interactive;
        in.vtAvailable = vt;
        QCOMPARE(shouldColor(in), expected);
    }

    void noColorEnvIgnoresEmptyValue() {
        qputenv("NO_COLOR", "");
        QVERIFY(!noColorEnvironmentSet());
        qputenv("NO_COLOR", "1");
        QVERIFY(noColorEnvironmentSet());
        qunsetenv("NO_COLOR");
        QVERIFY(!noColorEnvironmentSet());
    }
};

QTEST_GUILESS_MAIN(ConsoleViewTest)
#include "tst_consoleview.moc"
