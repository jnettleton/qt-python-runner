#pragma once

#include "services/IProcessRunner.h"
#include "views/IRunView.h"
#include "services/PythonLocator.h"
#include "models/RunResult.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <chrono>
#include <vector>

namespace pyrun {

struct RunOptions {
    QString python;                        // interpreter override; empty = auto-detect
    std::chrono::milliseconds timeout{0};  // per script; 0 = none
    bool failFast = false;                 // stop at the first failed script
};

struct BatchSummary {
    std::vector<RunResult> results;
    int skipped = 0;        // never started (fail-fast, or stopped)
    QString fatalError;     // nothing ran (no scripts / no interpreter)

    [[nodiscard]] int failedCount() const;
    // 0 = every script succeeded; 1 = at least one script failed (non-zero
    // exit, crash, failed to start, stopped, timed out); 2 = nothing could be
    // run (no scripts, no interpreter).
    [[nodiscard]] int exitCode() const;
};

// The one presenter for both apps. A GUI run is a batch of one.
// Holds all decisions: validation, sequencing, failure policy, messages.
// Talks only to IRunView / IProcessRunner / IInterpreterLocator.
//
// Ownership: references are non-owning; the composition root (main) owns the
// view, runner and locator and must outlive the presenter.
class RunPresenter : public QObject {
    Q_OBJECT
public:
    RunPresenter(IRunView& view, IProcessRunner& runner, const IInterpreterLocator& locator,
                 QObject* parent = nullptr);

    [[nodiscard]] bool isRunning() const { return m_running; }

public slots:
    void run(const QStringList& scripts, const pyrun::RunOptions& options);
    void stop();  // kills the current script and abandons the rest of the batch

signals:
    void batchFinished(const pyrun::BatchSummary& summary);

private:
    void startNext();
    void onOutput(const QString& text, OutputChannel channel);
    void onFinished(const RunResult& result);
    void handleResult(RunResult result);
    void finishBatch();
    void abortWith(const QString& fatalError);
    void ensureLineStart();
    [[nodiscard]] static StatusSeverity severityOf(const RunResult& result);
    [[nodiscard]] static QString validate(const QString& scriptPath);

    IRunView& m_view;
    IProcessRunner& m_runner;
    const IInterpreterLocator& m_locator;

    bool m_running = false;
    bool m_stopRequested = false;
    bool m_midLine = false;
    QString m_interpreter;
    RunOptions m_options;
    QStringList m_scripts;
    int m_index = -1;
    std::vector<RunResult> m_results;
};

}  // namespace pyrun

Q_DECLARE_METATYPE(pyrun::RunOptions)
Q_DECLARE_METATYPE(pyrun::BatchSummary)
