#include "ConsoleColor.h"
#include "ConsoleEncodingGuard.h"
#include "views/ConsoleView.h"
#include "models/FileList.h"
#include "services/ProcessRunner.h"
#include "services/PythonLocator.h"
#include "presenters/RunPresenter.h"
#include "pyrun/version.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

#include <cmath>
#include <cstdio>

namespace {

constexpr int kExitUsage = 2;

void print(std::FILE* stream, const QString& text) {
    const QByteArray bytes = text.toUtf8();
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stream);
}

int usageError(const QString& message) {
    print(stderr, QStringLiteral("%1: %2\nTry '%1 --help'.\n")
                      .arg(QString::fromLatin1(pyrun::kCliName), message));
    return kExitUsage;
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QString::fromLatin1(pyrun::kCliName));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(pyrun::kVersion));
    QCoreApplication::setOrganizationName(QString::fromLatin1(pyrun::kOrganization));

    const pyrun::ConsoleEncodingGuard encoding;

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Run Python scripts one after another and print their output.\n"
        "Script output goes to stdout (the script's stdout and stderr, in the order they were\n"
        "read; across the two pipes that order is approximate); headers, results and the\n"
        "summary go to stderr. When stdout is an interactive console the script's stderr text\n"
        "is shown in red; never when redirected, with NO_COLOR set, or with --no-color.\n"
        "\n"
        "Exit codes: 0 all scripts succeeded; 1 at least one failed (non-zero exit, crash,\n"
        "could not start, timed out) or was not run (--fail-fast); 2 usage error or nothing\n"
        "could be run (no scripts, no interpreter)."));
    const QCommandLineOption helpOption = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();
    const QCommandLineOption fileListOption(
        {QStringLiteral("f"), QStringLiteral("file-list")},
        QStringLiteral("Read script paths from <path>, one per line. Blank lines and lines "
                       "starting with '#' are ignored; relative paths are relative to the list "
                       "file. May be repeated; runs after any scripts given on the command line."),
        QStringLiteral("path"));
    const QCommandLineOption pythonOption(
        QStringLiteral("python"),
        QStringLiteral("Python interpreter to use (default: auto-detect on PATH)."),
        QStringLiteral("path"));
    const QCommandLineOption timeoutOption(
        QStringLiteral("timeout"),
        QStringLiteral("Kill a script that runs longer than <seconds> (default: no timeout)."),
        QStringLiteral("seconds"));
    const QCommandLineOption failFastOption(
        QStringLiteral("fail-fast"), QStringLiteral("Stop at the first failed script."));
    const QCommandLineOption noColorOption(
        QStringLiteral("no-color"),
        QStringLiteral("Never colour output (also off when redirected or NO_COLOR is set)."));
    parser.addOptions(
        {fileListOption, pythonOption, timeoutOption, failFastOption, noColorOption});
    parser.addPositionalArgument(QStringLiteral("scripts"),
                                 QStringLiteral("Python files to run, in order."),
                                 QStringLiteral("[scripts...]"));

    if (!parser.parse(QCoreApplication::arguments())) {
        return usageError(parser.errorText());
    }
    // Printed by us, not parser.showVersion()/showHelp(): those call exit(), which
    // would skip the console code page restore in ~ConsoleEncodingGuard.
    if (parser.isSet(versionOption)) {
        print(stdout, QStringLiteral("%1 %2\n")
                          .arg(QCoreApplication::applicationName(),
                               QCoreApplication::applicationVersion()));
        return 0;
    }
    if (parser.isSet(helpOption)) {
        print(stdout, parser.helpText());
        return 0;
    }

    pyrun::RunOptions options;
    options.python = parser.value(pythonOption);
    options.failFast = parser.isSet(failFastOption);
    if (parser.isSet(timeoutOption)) {
        bool ok = false;
        const double seconds = parser.value(timeoutOption).toDouble(&ok);
        if (!ok || !std::isfinite(seconds) || seconds <= 0.0 || seconds > 86400.0 * 365) {
            return usageError(QStringLiteral("--timeout expects a positive number of seconds"));
        }
        options.timeout = std::chrono::milliseconds(std::llround(seconds * 1000.0));
    }

    QStringList scripts;
    const QStringList positional = parser.positionalArguments();
    for (const QString& p : positional) {
        scripts.append(QDir::cleanPath(QFileInfo(p).absoluteFilePath()));
    }
    const QStringList lists = parser.values(fileListOption);
    for (const QString& listPath : lists) {
        const pyrun::FileListResult list = pyrun::readFileList(listPath);
        if (!list.ok()) {
            return usageError(list.error);
        }
        scripts.append(list.paths);
    }
    if (scripts.isEmpty()) {
        return usageError(QStringLiteral("no scripts given (pass .py files or --file-list)"));
    }

    // Composition root. Ownership: everything below lives on main()'s stack and
    // is destroyed in reverse order; the QProcess inside `runner` is its child.
    // Declared before the view: restores the console mode after the view is gone.
    const pyrun::ConsoleColorGuard colour(parser.isSet(noColorOption));
    pyrun::ConsoleView view(colour.stdoutColor(), colour.stderrColor());
    pyrun::PythonLocator locator;
    pyrun::ProcessRunner runner;
    pyrun::RunPresenter presenter(view, runner, locator);

    QObject::connect(&presenter, &pyrun::RunPresenter::batchFinished, &app,
                     [](const pyrun::BatchSummary& summary) {
                         QCoreApplication::exit(summary.exitCode());
                     });
    // Start once the event loop is running; the whole batch is event driven.
    QTimer::singleShot(0, &presenter,
                       [&presenter, scripts, options] { presenter.run(scripts, options); });

    return QCoreApplication::exec();
}
