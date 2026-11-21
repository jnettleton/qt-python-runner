#pragma once

#include <QMetaType>
#include <QString>

#include <chrono>

namespace pyrun {

enum class RunOutcome {
    Exited,         // process ran and exited on its own (exitCode is meaningful)
    Crashed,        // process died abnormally (QProcess::CrashExit) without us killing it
    FailedToStart,  // never ran: bad interpreter, missing script, invalid input
    Stopped,        // killed because the user asked (Stop button)
    TimedOut        // killed because --timeout expired
};

// Value type describing how one script run ended. No behaviour beyond queries.
struct RunResult {
    QString script;
    RunOutcome outcome = RunOutcome::FailedToStart;
    int exitCode = -1;
    std::chrono::milliseconds elapsed{0};
    bool producedOutput = false;
    QString message;  // detail for FailedToStart, plus any diagnostic hint

    [[nodiscard]] bool succeeded() const noexcept {
        return outcome == RunOutcome::Exited && exitCode == 0;
    }
};

// "1.23 s"
[[nodiscard]] QString formatElapsed(std::chrono::milliseconds elapsed);

// One-line, human-readable status, e.g. "[ok] exit code 0 (0.12 s)".
[[nodiscard]] QString formatResult(const RunResult& result);

}  // namespace pyrun

Q_DECLARE_METATYPE(pyrun::RunResult)
