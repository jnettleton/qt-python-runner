#include "services/PythonLocator.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <utility>

namespace pyrun {

namespace {

QStringList pathDirectories() {
    return qEnvironmentVariable("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);
}

QStringList candidateNames() {
#ifdef Q_OS_WIN
    return {QStringLiteral("python.exe"), QStringLiteral("python3.exe"),
            QStringLiteral("py.exe")};
#else
    return {QStringLiteral("python3"), QStringLiteral("python")};
#endif
}

const char* const kPyenvHint =
    "pyenv-win users: python on PATH is a .bat shim, which is deliberately not run "
    "(cmd.exe argument parsing). Find the real interpreter with 'pyenv which python' "
    "and pass that python.exe (--python <path> on the command line, the Interpreter "
    "field in the GUI).";

QString notFoundHelp(const QString& skippedBatchShim) {
    QString text = QStringLiteral(
        "No Python interpreter found on PATH (looked for python.exe, python3.exe, py.exe).\n"
        "Install Python from python.org, or point this tool at one explicitly "
        "(--python <path> on the command line, the Interpreter field in the GUI).\n"
        "Note: the Microsoft Store 'python' alias is not an interpreter.");
    if (!skippedBatchShim.isEmpty()) {
        text += QStringLiteral("\nSkipped batch shim: %1\n%2")
                    .arg(QDir::toNativeSeparators(skippedBatchShim),
                         QString::fromLatin1(kPyenvHint));
    }
    return text;
}

}  // namespace

PythonLocator::PythonLocator() : m_dirs(pathDirectories()) {}

PythonLocator::PythonLocator(QStringList searchDirs) : m_dirs(std::move(searchDirs)) {}

bool PythonLocator::isStoreAlias(const QString& path) {
    const QString p = QDir::fromNativeSeparators(path);
    return p.contains(QStringLiteral("/WindowsApps/"), Qt::CaseInsensitive);
}

bool PythonLocator::isBatchFile(const QString& path) {
    return path.endsWith(QStringLiteral(".bat"), Qt::CaseInsensitive) ||
           path.endsWith(QStringLiteral(".cmd"), Qt::CaseInsensitive);
}

InterpreterLookup PythonLocator::locate(const QString& userOverride) const {
    const QString value = userOverride.trimmed();
    return value.isEmpty() ? autoDetect() : resolveOverride(value);
}

InterpreterLookup PythonLocator::resolveOverride(const QString& rawValue) const {
    QString value = rawValue;
    if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) &&
        value.endsWith(QLatin1Char('"'))) {
        value = value.mid(1, value.size() - 2);
    }

    QString resolved;
    const QFileInfo info(value);
    if (info.isFile()) {
        resolved = info.absoluteFilePath();
    } else if (!value.contains(QLatin1Char('/')) && !value.contains(QLatin1Char('\\'))) {
        // A bare name such as "python3.12": search our directories only.
        for (const QString& dir : std::as_const(m_dirs)) {
            resolved = QStandardPaths::findExecutable(value, {dir});
            if (!resolved.isEmpty() && !isBatchFile(resolved)) {
                break;
            }
            resolved.clear();
        }
    }

    InterpreterLookup lookup;
    if (resolved.isEmpty()) {
        lookup.error = QStringLiteral("Python interpreter '%1' was not found.")
                           .arg(QDir::toNativeSeparators(value));
    } else if (isBatchFile(resolved)) {
        lookup.error = QStringLiteral(
                           "'%1' is a batch file. Batch files are not run directly (cmd.exe "
                           "argument parsing). Point at the real python.exe instead "
                           "(pyenv-win: 'pyenv which python').")
                           .arg(QDir::toNativeSeparators(resolved));
    } else {
        lookup.path = resolved;
    }
    return lookup;
}

InterpreterLookup PythonLocator::autoDetect() const {
    QString storeFallback;
    QString skippedBatch;
    for (const QString& name : candidateNames()) {
        for (const QString& dir : std::as_const(m_dirs)) {
            const QString found = QStandardPaths::findExecutable(name, {dir});
            if (found.isEmpty() || isBatchFile(found)) {
                continue;
            }
            const QString absolute = QFileInfo(found).absoluteFilePath();
            if (!isStoreAlias(absolute)) {
                return {absolute, {}};
            }
            if (storeFallback.isEmpty()) {
                storeFallback = absolute;
            }
        }
    }
    if (!storeFallback.isEmpty()) {
        return {storeFallback, {}};
    }
    // Report a batch shim only if it explains the failure (nothing else found).
    for (const QString& dir : std::as_const(m_dirs)) {
        for (const char* shim : {"python.bat", "python.cmd", "python3.bat", "py.bat"}) {
            const QString found = QStandardPaths::findExecutable(QString::fromLatin1(shim), {dir});
            if (!found.isEmpty()) {
                skippedBatch = found;
                break;
            }
        }
        if (!skippedBatch.isEmpty()) {
            break;
        }
    }
    return {{}, notFoundHelp(skippedBatch)};
}

}  // namespace pyrun
