#include "services/PythonLocator.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace pyrun;

class PythonLocatorTest : public QObject {
    Q_OBJECT

    static QString touch(const QString& path) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write("stub");
        }
        f.setPermissions(f.permissions() | QFileDevice::ExeUser | QFileDevice::ExeOwner);
        return path;
    }

private slots:
    void storeAliasDetection_data() {
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("expected");
        QTest::newRow("store")
            << "C:\\Users\\x\\AppData\\Local\\Microsoft\\WindowsApps\\python.exe" << true;
        QTest::newRow("store-fwd") << "c:/users/x/appdata/local/microsoft/windowsapps/py.exe" << true;
        QTest::newRow("real") << "C:\\Python313\\python.exe" << false;
    }
    void storeAliasDetection() {
        QFETCH(QString, path);
        QFETCH(bool, expected);
        QCOMPARE(PythonLocator::isStoreAlias(path), expected);
    }

    void overrideMustExist() {
        const PythonLocator loc(QStringList{});
        const InterpreterLookup r = loc.locate(QStringLiteral("C:/no/such/python.exe"));
        QVERIFY(!r.ok());
        QVERIFY(r.error.contains("was not found"));
    }

    void overrideRejectsBatchFiles() {
        QTemporaryDir dir;
        const QString bat = touch(dir.filePath("python.bat"));
        const InterpreterLookup r = PythonLocator(QStringList{}).locate(bat);
        QVERIFY(!r.ok());
        QVERIFY(r.error.contains("batch file"));
    }

    void overrideReturnsAbsolutePath() {
        QTemporaryDir dir;
        const QString exe = touch(dir.filePath("my python/python.exe"));
        const InterpreterLookup r = PythonLocator(QStringList{}).locate(exe);
        QVERIFY(r.ok());
        QVERIFY(QFileInfo(r.path).isAbsolute());
    }

#ifdef Q_OS_WIN
    void autoDetectPrefersRealOverStoreAndSkipsBatch() {
        QTemporaryDir dir;
        const QString storeDir = dir.filePath("AppData/Local/Microsoft/WindowsApps");
        const QString realDir = dir.filePath("Python313");
        const QString shimDir = dir.filePath("shims");
        touch(storeDir + "/python.exe");
        touch(shimDir + "/python.bat");
        touch(realDir + "/python.exe");

        // PATH order: store alias first, then batch shim, then the real one.
        const PythonLocator loc(QStringList{storeDir, shimDir, realDir});
        const InterpreterLookup r = loc.locate({});
        QVERIFY2(r.ok(), qPrintable(r.error));
        QCOMPARE(QFileInfo(r.path).absoluteFilePath(),
                 QFileInfo(realDir + "/python.exe").absoluteFilePath());
    }

    void onlyABatchShimGivesPyenvGuidance() {
        QTemporaryDir dir;
        touch(dir.filePath("python.bat"));
        const InterpreterLookup r = PythonLocator(QStringList{dir.path()}).locate({});
        QVERIFY(!r.ok());
        QVERIFY(r.error.contains("pyenv which python"));
        QVERIFY(r.error.contains("python.bat"));
    }

    void autoDetectFallsBackToStoreStub() {
        QTemporaryDir dir;
        const QString storeDir = dir.filePath("WindowsApps");
        touch(storeDir + "/python.exe");
        const InterpreterLookup r = PythonLocator(QStringList{storeDir}).locate({});
        QVERIFY(r.ok());
        QVERIFY(PythonLocator::isStoreAlias(r.path));
    }
#endif

    void autoDetectNotFoundIsActionable() {
        QTemporaryDir dir;
        const InterpreterLookup r = PythonLocator(QStringList{dir.path()}).locate({});
        QVERIFY(!r.ok());
        QVERIFY(r.error.contains("--python"));
        QVERIFY(r.error.contains("Microsoft Store"));
    }
};

QTEST_GUILESS_MAIN(PythonLocatorTest)
#include "tst_pythonlocator.moc"
