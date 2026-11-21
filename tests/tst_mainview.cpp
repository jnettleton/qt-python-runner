// Widget smoke test (offscreen): the view forwards intent and mirrors state.
// Deliberately tiny; behaviour is tested at the presenter.
#include "views/AboutView.h"
#include "views/MainView.h"
#include "views/OutputFormats.h"
#include "pyrun/version.h"

#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QSignalSpy>
#include <QtTest>

using namespace pyrun;

class MainViewTest : public QObject {
    Q_OBJECT
private slots:
    void runClickForwardsPathsWithoutDeciding() {
        MainView w;
        QSignalSpy spy(&w, &MainView::runRequested);
        w.findChild<QLineEdit*>("scriptEdit")->setText("  C:/x/a.py ");
        w.findChild<QLineEdit*>("pythonEdit")->setText("C:/py/python.exe");
        w.findChild<QPushButton*>("runButton")->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QStringLiteral("C:/x/a.py"));
        QCOMPARE(spy.first().at(1).toString(), QStringLiteral("C:/py/python.exe"));
    }

    void stopClickForwards() {
        MainView w;
        w.setRunning(true);
        QSignalSpy spy(&w, &MainView::stopRequested);
        w.findChild<QPushButton*>("stopButton")->click();
        QCOMPARE(spy.count(), 1);
    }

    void setRunningTogglesControls() {
        MainView w;
        auto* run = w.findChild<QPushButton*>("runButton");
        auto* stop = w.findChild<QPushButton*>("stopButton");
        QVERIFY(run->isEnabled() && !stop->isEnabled());
        w.setRunning(true);
        QVERIFY(!run->isEnabled() && stop->isEnabled());
        w.setRunning(false);
        QVERIFY(run->isEnabled() && !stop->isEnabled());
    }

    void outputAndStatusAppend() {
        MainView w;
        w.showOutput("par", OutputChannel::StdOut);
        w.showOutput("tial\n", OutputChannel::StdOut);
        w.showStatus("[ok] exit code 0", StatusSeverity::Success);
        QCOMPARE(w.findChild<QPlainTextEdit*>("outputView")->toPlainText(),
                 QStringLiteral("partial\n[ok] exit code 0\n"));
    }

    // The colour of one character in the output pane (position is 0-based).
    static QTextCharFormat formatAt(MainView& w, int position) {
        QTextCursor cursor(w.findChild<QPlainTextEdit*>("outputView")->document());
        cursor.setPosition(position + 1);  // charFormat() is the char before the cursor
        return cursor.charFormat();
    }

    void stderrTextGetsTheStderrFormatAndStdoutStaysDefault() {
        MainView w;
        w.showOutput("out\n", OutputChannel::StdOut);
        w.showOutput("err\n", OutputChannel::StdErr);
        w.showOutput("out2\n", OutputChannel::StdOut);  // red must not bleed forward

        const QPalette palette = w.findChild<QPlainTextEdit*>("outputView")->palette();
        const QTextCharFormat out = formatAt(w, 0);
        const QTextCharFormat err = formatAt(w, 4);
        const QTextCharFormat out2 = formatAt(w, 8);

        QVERIFY(!out.hasProperty(QTextFormat::ForegroundBrush));  // palette decides
        QVERIFY(err.hasProperty(QTextFormat::ForegroundBrush));
        QCOMPARE(err.foreground().color(), stderrTextColor(palette));
        QVERIFY(!out2.hasProperty(QTextFormat::ForegroundBrush));
    }

    void failureStatusGetsTheStderrRedAndOkStaysPlain() {
        MainView w;
        w.showStatus("[ok] exit 0", StatusSeverity::Success);      // 12 chars incl. \n
        w.showStatus("[failed] exit 1", StatusSeverity::Failure);
        w.showStatus("Running x", StatusSeverity::Info);

        const QPalette palette = w.findChild<QPlainTextEdit*>("outputView")->palette();
        const QTextCharFormat ok = formatAt(w, 0);
        const QTextCharFormat failed = formatAt(w, int(QStringLiteral("[ok] exit 0\n").size()));
        const QTextCharFormat info =
            formatAt(w, int(QStringLiteral("[ok] exit 0\n[failed] exit 1\n").size()));
        QVERIFY(!ok.hasProperty(QTextFormat::ForegroundBrush));
        QVERIFY(failed.hasProperty(QTextFormat::ForegroundBrush));
        // The same colour as stderr text: one constant, not a second one.
        QCOMPARE(failed.foreground().color(), stderrTextColor(palette));
        QCOMPARE(failed.foreground(), formatFor(OutputChannel::StdErr, palette).foreground());
        QVERIFY(!info.hasProperty(QTextFormat::ForegroundBrush));
    }

    void errorsAndTheStatusBarUseTheFailureColour() {
        MainView w;
        const QPalette palette = w.findChild<QPlainTextEdit*>("outputView")->palette();
        w.showError("no interpreter");
        QCOMPARE(formatAt(w, 0).foreground().color(), stderrTextColor(palette));
        QCOMPARE(w.statusBar()->palette().color(QPalette::WindowText), stderrTextColor(palette));

        w.showStatus("[ok] exit 0", StatusSeverity::Success);  // resets the status bar
        QVERIFY(w.statusBar()->palette().color(QPalette::WindowText) != stderrTextColor(palette));
    }

    // Lightness is pinned per palette so any hue (red now, yellow later) reads
    // on both: deep on a light base, pale on a dark base.
    void stderrColourLightnessFollowsThePalette() {
        QPalette light;
        light.setColor(QPalette::Base, Qt::white);
        QPalette dark;
        dark.setColor(QPalette::Base, QColor(0x1e, 0x1e, 0x1e));
        QVERIFY(stderrTextColor(light).lightnessF() < 0.5);
        QVERIFY(stderrTextColor(dark).lightnessF() > 0.5);
    }

    void aboutDialogShowsVersionFromTheSingleSource() {
        AboutView d;
        QVERIFY(d.findChild<QLabel*>("aboutVersion")->text().contains(
            QString::fromLatin1(kVersion)));
        QCOMPARE(d.findChild<QLabel*>("aboutName")->text(), QString::fromLatin1(kGuiName));
    }

    void aboutBoxCarriesTheQtAttribution() {
        AboutView d;
        const auto* notice = d.findChild<QLabel*>("aboutQtNotice");
        QVERIFY(notice != nullptr);
        QVERIFY(notice->text().contains(QString::fromLatin1(qVersion())));  // runtime version
        QVERIFY(notice->text().contains(QStringLiteral("GNU LGPL v3")));
        QVERIFY(notice->text().contains(QStringLiteral("https://www.qt.io/licensing/")));
        QVERIFY(notice->openExternalLinks());
        QVERIFY(d.findChild<QPushButton*>("aboutQtButton") != nullptr);
        // The product name must not contain Qt's trademark.
        QVERIFY(!QString::fromLatin1(kGuiName).contains(QStringLiteral("Qt")));
        QVERIFY(!QString::fromLatin1(kCliName).contains(QStringLiteral("Qt")));
    }
};

QTEST_MAIN(MainViewTest)
#include "tst_mainview.moc"
