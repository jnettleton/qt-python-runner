#include "presenters/RunPresenter.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace pyrun {

using namespace std::chrono_literals;

int BatchSummary::failedCount() const {
    return static_cast<int>(std::count_if(results.begin(), results.end(),
                                          [](const RunResult& r) { return !r.succeeded(); }));
}

int BatchSummary::exitCode() const {
    if (!fatalError.isEmpty()) {
        return 2;
    }
    return (failedCount() > 0 || skipped > 0) ? 1 : 0;
}

RunPresenter::RunPresenter(IRunView& view, IProcessRunner& runner,
                           const IInterpreterLocator& locator, QObject* parent)
    : QObject(parent), m_view(view), m_runner(runner), m_locator(locator) {
    connect(&m_runner, &IProcessRunner::outputReceived, this, &RunPresenter::onOutput);
    connect(&m_runner, &IProcessRunner::finished, this, &RunPresenter::onFinished);
}

// Anything but a clean exit 0 is a failure: non-zero exit, crash, failed to
// start, stopped, timed out.
StatusSeverity RunPresenter::severityOf(const RunResult& result) {
    return result.succeeded() ? StatusSeverity::Success : StatusSeverity::Failure;
}

QString RunPresenter::validate(const QString& scriptPath) {
    if (scriptPath.trimmed().isEmpty()) {
        return QStringLiteral("No script selected.");
    }
    const QFileInfo info(scriptPath);
    if (!info.isFile()) {
        return QStringLiteral("Script not found: %1")
            .arg(QDir::toNativeSeparators(info.absoluteFilePath()));
    }
    if (info.suffix().compare(QStringLiteral("py"), Qt::CaseInsensitive) != 0) {
        return QStringLiteral("Not a .py file: %1")
            .arg(QDir::toNativeSeparators(info.absoluteFilePath()));
    }
    return {};
}

void RunPresenter::run(const QStringList& scripts, const RunOptions& options) {
    if (m_running) {
        m_view.showError(QStringLiteral("A run is already in progress."));
        return;
    }
    m_view.clearOutput();
    m_options = options;
    m_scripts = scripts;
    m_results.clear();
    m_index = -1;
    m_stopRequested = false;
    m_midLine = false;

    if (m_scripts.isEmpty()) {
        abortWith(QStringLiteral("No scripts to run."));
        return;
    }
    const InterpreterLookup lookup = m_locator.locate(options.python);
    if (!lookup.ok()) {
        abortWith(lookup.error);
        return;
    }
    m_interpreter = lookup.path;
    m_running = true;
    m_view.setRunning(true);
    startNext();
}

void RunPresenter::stop() {
    if (!m_running) {
        return;
    }
    m_stopRequested = true;
    m_runner.stop();
}

void RunPresenter::abortWith(const QString& fatalError) {
    m_view.showError(fatalError);
    BatchSummary summary;
    summary.fatalError = fatalError;
    emit batchFinished(summary);
}

void RunPresenter::startNext() {
    ++m_index;
    if (m_index >= m_scripts.size()) {
        finishBatch();
        return;
    }

    const QString path = m_scripts.at(m_index);
    const QString shown = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    const QString prefix = m_scripts.size() > 1
                               ? QStringLiteral("[%1/%2] ").arg(m_index + 1).arg(m_scripts.size())
                               : QString();
    ensureLineStart();
    m_view.showStatus(QStringLiteral("%1Running %2").arg(prefix, shown), StatusSeverity::Info);
    m_view.showStatus(
        QStringLiteral("  interpreter: %1").arg(QDir::toNativeSeparators(m_interpreter)),
        StatusSeverity::Info);

    if (const QString problem = validate(path); !problem.isEmpty()) {
        RunResult bad;
        bad.script = path;
        bad.outcome = RunOutcome::FailedToStart;
        bad.message = problem;
        handleResult(std::move(bad));
        return;
    }

    RunRequest request;
    request.interpreter = m_interpreter;
    request.script = QFileInfo(path).absoluteFilePath();
    request.timeout = m_options.timeout;
    m_runner.start(request);
}

void RunPresenter::onOutput(const QString& text, OutputChannel channel) {
    if (!m_running || text.isEmpty()) {
        return;
    }
    m_midLine = !text.endsWith(QLatin1Char('\n'));
    m_view.showOutput(text, channel);
}

void RunPresenter::onFinished(const RunResult& result) {
    if (!m_running) {
        return;
    }
    RunResult r = result;
    // The Microsoft Store stub exits at once with no output. Say what it is.
    if (r.outcome == RunOutcome::Exited && r.exitCode != 0 && !r.producedOutput &&
        r.elapsed < 3000ms && PythonLocator::isStoreAlias(m_interpreter)) {
        r.message = QStringLiteral(
            "The interpreter exited immediately with no output, and it lives under "
            "WindowsApps. That is likely the Microsoft Store 'python' stub, not a real "
            "Python. Install Python from python.org, or choose a real interpreter "
            "(--python <path> / the Interpreter field).");
    }
    handleResult(std::move(r));
}

void RunPresenter::handleResult(RunResult result) {
    ensureLineStart();
    m_view.showStatus(formatResult(result), severityOf(result));
    if (result.outcome == RunOutcome::Stopped) {
        m_stopRequested = true;
    }
    const bool failed = !result.succeeded();
    m_results.push_back(std::move(result));

    if (m_stopRequested || (m_options.failFast && failed)) {
        finishBatch();
        return;
    }
    startNext();
}

void RunPresenter::finishBatch() {
    BatchSummary summary;
    summary.results = m_results;
    summary.skipped = static_cast<int>(m_scripts.size()) - static_cast<int>(m_results.size());
    m_running = false;

    if (m_scripts.size() > 1 || summary.skipped > 0) {
        ensureLineStart();
        m_view.showStatus(QStringLiteral("Summary:"), StatusSeverity::Info);
        for (const RunResult& r : summary.results) {
            m_view.showStatus(
                QStringLiteral("  %1  %2")
                    .arg(r.succeeded() ? QStringLiteral("ok    ") : QStringLiteral("FAILED"),
                         QDir::toNativeSeparators(r.script)),
                severityOf(r));
        }
        const int failedCount = summary.failedCount();
        const int okCount = static_cast<int>(summary.results.size()) - failedCount;
        m_view.showStatus(QStringLiteral("%1 succeeded, %2 failed, %3 not run")
                              .arg(okCount)
                              .arg(failedCount)
                              .arg(summary.skipped),
                          (failedCount > 0 || summary.skipped > 0) ? StatusSeverity::Failure
                                                                    : StatusSeverity::Success);
    }
    m_view.setRunning(false);
    emit batchFinished(summary);
}

void RunPresenter::ensureLineStart() {
    if (m_midLine) {
        m_midLine = false;
        m_view.showOutput(QStringLiteral("\n"), OutputChannel::StdOut);
    }
}

}  // namespace pyrun
