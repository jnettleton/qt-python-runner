#include "services/ProcessRunner.h"

#include "services/ProcessTree.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

#include <utility>

namespace pyrun {

ProcessRunner::ProcessRunner(QObject* parent)
    : IProcessRunner(parent), m_timeout(this), m_killGrace(this) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, &ProcessRunner::onTimeout);
    m_killGrace.setSingleShot(true);
    m_killGrace.setInterval(kKillGrace);
    connect(&m_killGrace, &QTimer::timeout, this, &ProcessRunner::onKillGrace);
}

ProcessRunner::~ProcessRunner() {
    if (m_process != nullptr) {
        // Last resort: no signals reach anyone from here. QProcess' own
        // destructor reaps the killed child.
        m_process->disconnect(this);
        killTree();
    }
}

void ProcessRunner::start(const RunRequest& request) {
    if (m_process != nullptr) {
        RunResult busy;
        busy.script = request.script;
        busy.outcome = RunOutcome::FailedToStart;
        busy.message = QStringLiteral("A script is already running.");
        emit finished(busy);
        return;
    }

    m_request = request;
    m_outDecoder.reset();
    m_errDecoder.reset();
    m_producedOutput = false;
    m_stopRequested = false;
    m_timedOut = false;
    m_job = std::make_unique<ProcessTree>();

    auto* process = new QProcess(this);
    m_process = process;
    process->setProcessChannelMode(QProcess::SeparateChannels);
    ProcessTree::prepare(*process);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    process->setProcessEnvironment(env);
    process->setWorkingDirectory(QFileInfo(request.script).absolutePath());

    connect(process, &QProcess::readyReadStandardOutput, this,
            [this] { scheduleDrain(); });
    connect(process, &QProcess::readyReadStandardError, this,
            [this] { scheduleDrain(); });
    connect(process, &QProcess::started, this, &ProcessRunner::onStarted);
    connect(process, &QProcess::finished, this, &ProcessRunner::onFinished);
    connect(process, &QProcess::errorOccurred, this, &ProcessRunner::onErrorOccurred);

    if (request.timeout.count() > 0) {
        m_timeout.start(request.timeout);
    }
    m_clock.start();

    // Argument list, never a joined command string. -u: unbuffered.
    process->start(request.interpreter,
                   {QStringLiteral("-u"), QDir::toNativeSeparators(request.script)});
    // A synchronous FailedToStart has already completed the run (m_process null).
    if (m_process != nullptr) {
        process->closeWriteChannel();
    }
}

void ProcessRunner::stop() {
    if (m_process == nullptr) {
        return;
    }
    m_stopRequested = true;
    killTree();
}

void ProcessRunner::onTimeout() {
    if (m_process == nullptr) {
        return;
    }
    m_timedOut = true;
    killTree();
}

void ProcessRunner::onStarted() {
    if (m_process != nullptr && m_job != nullptr && !m_job->adopted()) {
        (void)m_job->adopt(m_process->processId());
    }
}

// Stops the interpreter and everything it spawned. With a graceful platform
// form (POSIX SIGTERM to the group) the hard kill follows after kKillGrace
// unless the run completes first; otherwise it is immediate.
void ProcessRunner::killTree() {
    if (m_process == nullptr) {
        return;
    }
    bool graceful = false;
    if (m_job != nullptr) {
        if (!m_job->adopted()) {
            (void)m_job->adopt(m_process->processId());
        }
        graceful = m_job->requestStop();
    }
    if (graceful) {
        if (!m_killGrace.isActive()) {
            m_killGrace.start();
        }
    } else {
        m_process->kill();  // tree unavailable (fallback), or already hard-killed (Windows)
    }
}

void ProcessRunner::onKillGrace() {
    if (m_process == nullptr) {
        return;
    }
    if (m_job != nullptr) {
        m_job->kill();
    }
    m_process->kill();
}

// Reads whatever is buffered on one channel, decodes it with that channel's
// own decoder and emits it tagged.
void ProcessRunner::drain(QProcess::ProcessChannel channel) {
    if (m_process == nullptr) {
        return;
    }
    const bool isErr = channel == QProcess::StandardError;
    m_process->setReadChannel(channel);
    const QByteArray data = m_process->readAll();
    if (data.isEmpty()) {
        return;
    }
    m_producedOutput = true;
    const QString text = (isErr ? m_errDecoder : m_outDecoder).decode(data);
    if (!text.isEmpty()) {
        emit outputReceived(text, isErr ? OutputChannel::StdErr : OutputChannel::StdOut);
    }
}

// The pipes are independent, so true cross-channel order is unknowable. Best
// effort: whichever channel signalled, take everything pending, stdout first.
// Why not "the channel that signalled first": observed on Windows, Qt signals
// stderr first even when the script wrote stdout first, which put a script's
// greeting after its stderr line. Stdout-first as the tie-break matches the
// common shape of a script (output, then a warning/traceback). It is a
// heuristic: when both pipes hold data at once, all of stdout precedes all of
// stderr, so a script that alternated rapidly is grouped, not interleaved, and
// stderr-then-stdout output that arrived together is reversed.
void ProcessRunner::scheduleDrain() {
    // Qt's two pipe readers complete independently. Reading the moment one
    // signals sees only that pipe's data (observed: stderr first, stdout empty,
    // although the script wrote stdout first). Hopping once through the event
    // loop lets the other reader's pending completion land first. No waiting.
    if (m_drainScheduled) {
        return;
    }
    m_drainScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_drainScheduled = false;
        drainPending();
    });
}

void ProcessRunner::drainPending() {
    drain(QProcess::StandardOutput);
    drain(QProcess::StandardError);
}

void ProcessRunner::onFinished(int exitCode, QProcess::ExitStatus status) {
    if (m_process == nullptr) {
        return;
    }
    drainPending();
    for (const QProcess::ProcessChannel channel :
         {QProcess::StandardOutput, QProcess::StandardError}) {
        const bool isErr = channel == QProcess::StandardError;
        const QString tail = (isErr ? m_errDecoder : m_outDecoder).flush();
        if (!tail.isEmpty()) {
            emit outputReceived(tail, isErr ? OutputChannel::StdErr : OutputChannel::StdOut);
        }
    }

    RunResult result;
    result.exitCode = exitCode;
    if (m_timedOut) {
        result.outcome = RunOutcome::TimedOut;
    } else if (m_stopRequested) {
        result.outcome = RunOutcome::Stopped;
    } else if (status == QProcess::CrashExit) {
        result.outcome = RunOutcome::Crashed;
    } else {
        result.outcome = RunOutcome::Exited;
    }
    complete(std::move(result));
}

void ProcessRunner::onErrorOccurred(QProcess::ProcessError error) {
    // Only FailedToStart needs handling here: it never emits finished().
    // Crashed is followed by finished(); other errors are informational.
    if (error != QProcess::FailedToStart || m_process == nullptr) {
        return;
    }
    RunResult result;
    result.outcome = RunOutcome::FailedToStart;
    result.message = QStringLiteral("cannot start '%1': %2")
                         .arg(QDir::toNativeSeparators(m_request.interpreter),
                              m_process->errorString());
    complete(std::move(result));
}

void ProcessRunner::complete(RunResult result) {
    result.script = m_request.script;
    result.elapsed = std::chrono::milliseconds(m_clock.elapsed());
    result.producedOutput = m_producedOutput;

    m_timeout.stop();
    m_killGrace.stop();
    QProcess* process = m_process;
    m_process = nullptr;  // idle before emitting, so a slot may start() the next run
    process->disconnect(this);
    process->deleteLater();
    m_job.reset();  // kills any leftover descendants (job closed / group SIGKILLed)

    emit finished(result);
}

}  // namespace pyrun
