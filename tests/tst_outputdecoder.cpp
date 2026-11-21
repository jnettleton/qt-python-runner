#include "services/OutputDecoder.h"

#include <QtTest>

using namespace pyrun;

class OutputDecoderTest : public QObject {
    Q_OBJECT
private slots:
    void plainAscii() {
        OutputDecoder d;
        QCOMPARE(d.decode("hello\n"), QStringLiteral("hello\n"));
    }

    // Every possible split point of a multi-byte string must decode identically.
    void utf8SplitAtEveryByte() {
        const QString expected = QStringLiteral("Grüße – 日本語 – ✓\n");
        const QByteArray bytes = expected.toUtf8();
        for (qsizetype cut = 1; cut < bytes.size(); ++cut) {
            OutputDecoder d;
            QString got = d.decode(QByteArrayView(bytes).first(cut));
            got += d.decode(QByteArrayView(bytes).sliced(cut));
            QVERIFY2(got == expected, qPrintable(QStringLiteral("cut at %1").arg(cut)));
        }
    }

    void utf8OneByteAtATime() {
        const QString expected = QStringLiteral("日本語 ✓");
        const QByteArray bytes = expected.toUtf8();
        OutputDecoder d;
        QString got;
        for (char b : bytes) {
            got += d.decode(QByteArrayView(&b, 1));
        }
        QCOMPARE(got, expected);
    }

    void crlfBecomesLf() {
        OutputDecoder d;
        QCOMPARE(d.decode("a\r\nb\r\n"), QStringLiteral("a\nb\n"));
    }

    void crlfSplitAcrossChunks() {
        OutputDecoder d;
        QString got = d.decode("a\r");
        got += d.decode("\nb");
        QCOMPARE(got, QStringLiteral("a\nb"));
    }

    void loneCrIsKept() {
        OutputDecoder d;
        QString got = d.decode("50%\r");
        got += d.decode("100%\n");
        QCOMPARE(got, QStringLiteral("50%\r100%\n"));
    }

    void trailingCrReleasedByFlush() {
        OutputDecoder d;
        QCOMPARE(d.decode("x\r"), QStringLiteral("x"));
        QCOMPARE(d.flush(), QStringLiteral("\r"));
        QCOMPARE(d.flush(), QString());
    }

    // Two decoders (stdout/stderr) fed alternately, each split mid-character:
    // neither may see the other's bytes.
    void decodersAreIndependent() {
        const QByteArray a = QStringLiteral("ü").toUtf8();   // 2 bytes
        const QByteArray b = QStringLiteral("日").toUtf8();  // 3 bytes
        OutputDecoder out;
        OutputDecoder err;
        QString gotOut = out.decode(QByteArrayView(a).first(1));
        QString gotErr = err.decode(QByteArrayView(b).first(1));
        gotOut += out.decode(QByteArrayView(a).sliced(1));
        gotErr += err.decode(QByteArrayView(b).sliced(1, 1));
        gotErr += err.decode(QByteArrayView(b).sliced(2));
        QCOMPARE(gotOut, QStringLiteral("ü"));
        QCOMPARE(gotErr, QStringLiteral("日"));
    }

    // A held CR on one channel must not leak into the other.
    void heldCrStaysOnItsChannel() {
        OutputDecoder out;
        OutputDecoder err;
        QCOMPARE(out.decode("a\r"), QStringLiteral("a"));
        QCOMPARE(err.decode("\nb"), QStringLiteral("\nb"));
        QCOMPARE(out.decode("\n"), QStringLiteral("\n"));
    }

    void resetDropsState() {
        OutputDecoder d;
        const QByteArray bytes = QStringLiteral("日").toUtf8();
        (void)d.decode(QByteArrayView(bytes).first(1));  // dangling lead byte
        d.reset();
        QCOMPARE(d.decode("ok"), QStringLiteral("ok"));
    }
};

QTEST_GUILESS_MAIN(OutputDecoderTest)
#include "tst_outputdecoder.moc"
