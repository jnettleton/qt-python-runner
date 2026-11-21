#include "services/ProcessRunner.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

#include <utility>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace pyrun {

// Owns one Windows Job Object configured with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE.
// Processes assigned to it (and their descendants) die when terminate() is
// called or the last handle closes (destructor, or this process dying).
// Elsewhere it compiles to no-ops. Known race: a child is assigned after it
// has started, so a descendant spawned in those first microseconds escapes;
// the interpreter's start-up time makes that practically unreachable.
class ProcessJob {
public:
    ProcessJob() {
#ifdef Q_OS_WIN
        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        if (job == nullptr) {
            return;
        }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info,
                                     sizeof(info))) {
            CloseHandle(job);
            return;
        }
        m_handle = job;
#endif
    }
    ~ProcessJob() {
#ifdef Q_OS_WIN
        if (m_handle != nullptr) {
            CloseHandle(static_cast<HANDLE>(m_handle));  // KILL_ON_JOB_CLOSE reaps the tree
        }
#endif
    }
    ProcessJob(const ProcessJob&) = delete;
    ProcessJob& operator=(const ProcessJob&) = delete;

    // Best effort: failure leaves plain kill() of the direct child as fallback.
    bool assign([[maybe_unused]] qint64 pid) {
#ifdef Q_OS_WIN
        if (m_handle == nullptr) {
            return false;
        }
        HANDLE process =
            OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (process == nullptr) {
            return false;
        }
        const BOOL ok = AssignProcessToJobObject(static_cast<HANDLE>(m_handle), process);
        CloseHandle(process);
        return ok != FALSE;
#else
        return false;
#endif
    }

    void terminate() {
#ifdef Q_OS_WIN
        if (m_handle != nullptr) {
            TerminateJobObject(static_cast<HANDLE>(m_handle), 1);
        }
#endif
    }

private:
    [[maybe_unused]] void* m_handle = nullptr;
};

ProcessRunner::ProcessRunner(QObject* parent) : IProcessRunner(parent), m_timeout(this) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, &ProcessRunner::onTimeout);
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
    m_job = std::make_unique<ProcessJob>();

    auto* process = new QProcess(this);
    m_process = process;
    process->setProcessChannelMode(QProcess::SeparateChannels);

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
    if (m_process != nullptr && m_job != nullptr) {
        (void)m_job->assign(m_process->processId());
    }
}

void ProcessRunner::killTree() {
    if (m_process == nullptr) {
        return;
    }
    if (m_job != nullptr) {
        m_job->terminate();  // interpreter + every descendant
    }
    m_process->kill();       // fallback if the job could not be used
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
    QProcess* process = m_process;
    m_process = nullptr;  // idle before emitting, so a slot may start() the next run
    process->disconnect(this);
    process->deleteLater();
    m_job.reset();  // closes the job: any leftover descendants are killed

    emit finished(result);
}

}  // namespace pyrun
