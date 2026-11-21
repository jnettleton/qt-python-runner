#include "models/FileList.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace pyrun;

class FileListTest : public QObject {
    Q_OBJECT
private slots:
    void blanksCommentsQuotesAndRelativePaths() {
        const QDir base(QDir::cleanPath(QDir::tempPath()));
        const QString text = QStringLiteral(
            "# header comment\r\n"
            "\r\n"
            "  a.py  \r\n"
            "   # indented comment\n"
            "\"with space/b.py\"\n"
            "sub/c#1.py\n"      // '#' mid-line is part of the name
            "\"\"\n"            // empty quotes -> skipped
            "\n");
        const QStringList got = parseFileList(text, base);
        const QStringList want{base.absoluteFilePath("a.py"),
                               base.absoluteFilePath("with space/b.py"),
                               base.absoluteFilePath("sub/c#1.py")};
        QCOMPARE(got, want);
    }

    void absolutePathsAreKept() {
        const QDir base(QDir::tempPath());
        const QString abs = QDir::cleanPath(QDir::current().absoluteFilePath("x/y.py"));
        QCOMPARE(parseFileList(abs, base), QStringList{abs});
    }

    void emptyListIsEmpty() {
        QVERIFY(parseFileList(QString(), QDir::current()).isEmpty());
        QVERIFY(parseFileList(QStringLiteral("\n  \n# only comments\n"), QDir::current()).isEmpty());
    }

    void readsUtf8WithBomAndResolvesAgainstListDirectory() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString listPath = dir.filePath("list.txt");
        QFile f(listPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("\xEF\xBB\xBF");
        f.write(QStringLiteral("# c\nsкрипт/日本語.py\n").toUtf8());
        f.close();

        const FileListResult r = readFileList(listPath);
        QVERIFY(r.ok());
        QCOMPARE(r.paths, QStringList{QDir(dir.path()).absoluteFilePath("sкрипт/日本語.py")});
    }

    void missingListFileReportsError() {
        const FileListResult r = readFileList(QDir::temp().filePath("definitely-not-here-123.txt"));
        QVERIFY(!r.ok());
        QVERIFY(r.error.contains("Cannot read file list"));
    }
};

QTEST_GUILESS_MAIN(FileListTest)
#include "tst_filelist.moc"
