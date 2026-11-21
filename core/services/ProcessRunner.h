#pragma once

#include "services/IProcessRunner.h"
#include "services/OutputDecoder.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QProcess>
#include <QTimer>

#include <chrono>
#include <memory>

namespace pyrun {

class ProcessTree;  // platform tree-kill (Job Object / process group), see ProcessTree.h

// Real IProcessRunner over QProcess.
//
// Ownership ("who's the parent?"):
//  - ProcessRunner is owned by whoever constructs it (main()'s scope / a parent).
//  - The QProcess is created per run with `this` as parent, detached from our
//    slots and deleteLater()'d when the run completes. It never outlives us.
//  - The timeout QTimer and the kill-grace QTimer are members (parent: this).
//  - The per-run ProcessTree (Job Object on Windows, process group on POSIX) is
//    owned by this runner (unique_ptr) and covers the interpreter plus
//    everything it spawns. It is destroyed when the run completes, which also
//    kills any leftover descendants: a script's children do not outlive its run.
//  - Who kills a hung child: the timeout timer or stop(), through the tree.
//    Windows: TerminateJobObject at once, then kill() (a console Python ignores
//    terminate()'s WM_CLOSE). POSIX: SIGTERM to the group, SIGKILL to the group
//    after kKillGrace if the run has not completed. The destructor kills the
//    tree as a last resort.
//  - Who hears about death: finished(RunResult), exactly once per start().
//
// Child setup: absolute interpreter, `-u script`, PYTHONUTF8=1,
// PYTHONUNBUFFERED=1, working directory = script directory,
// SeparateChannels (output is tagged stdout/stderr), stdin closed (script sees EOF).
class ProcessRunner final : public IProcessRunner {
    Q_OBJECT
public:
    // How long a POSIX process group gets between SIGTERM and SIGKILL.
    static constexpr std::chrono::milliseconds kKillGrace{2000};

    explicit ProcessRunner(QObject* parent = nullptr);
    ~ProcessRunner() override;

    void start(const RunRequest& request) override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override { return m_process != nullptr; }

private:
    void drain(QProcess::ProcessChannel channel);
    void scheduleDrain();
    void drainPending();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onErrorOccurred(QProcess::ProcessError error);
    void onTimeout();
    void onKillGrace();
    void onStarted();
    void killTree();
    void complete(RunResult result);

    RunRequest m_request;
    QProcess* m_process = nullptr;  // child of this; reset to nullptr when run completes
    OutputDecoder m_outDecoder;  // one stateful decoder per channel: a multi-byte
    OutputDecoder m_errDecoder;  // sequence split on one pipe must not touch the other
    QElapsedTimer m_clock;
    QTimer m_timeout;
    QTimer m_killGrace;  // POSIX: SIGTERM -> SIGKILL escalation
    std::unique_ptr<ProcessTree> m_job;
    bool m_producedOutput = false;
    bool m_drainScheduled = false;
    bool m_stopRequested = false;
    bool m_timedOut = false;
};

}  // namespace pyrun
