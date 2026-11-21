#pragma once

#include "services/IProcessRunner.h"
#include "views/IRunView.h"
#include "services/PythonLocator.h"

#include <QStringList>

#include <utility>
#include <vector>

namespace pyrun::testing {

class FakeView final : public IRunView {
public:
    void showOutput(const QString& text, OutputChannel channel) override {
        output += text;
        chunks.push_back({text, channel});
    }
    void showStatus(const QString& line, StatusSeverity severity) override {
        statuses << line;
        statusSeverities.push_back(severity);
        output += line + QLatin1Char('\n');  // what a real view would append
    }
    void showError(const QString& message) override { errors << message; }
    void setRunning(bool running) override { runningHistory.push_back(running); }
    void clearOutput() override { ++clearCount; output.clear(); }

    QString output;
    std::vector<std::pair<QString, OutputChannel>> chunks;  // showOutput calls, tagged
    QStringList statuses;
    std::vector<StatusSeverity> statusSeverities;  // parallel to statuses
    QStringList errors;
    std::vector<bool> runningHistory;
    int clearCount = 0;
};

class FakeRunner final : public IProcessRunner {
    // No Q_OBJECT: signals are inherited from IProcessRunner (and this header is
    // not run through moc).
public:
    void start(const RunRequest& request) override {
        requests.push_back(request);
        m_running = true;
    }
    void stop() override { ++stopCalls; }
    [[nodiscard]] bool isRunning() const override { return m_running; }

    void emitOutput(const QString& text, OutputChannel channel = OutputChannel::StdOut) {
        emit outputReceived(text, channel);
    }
    void complete(RunOutcome outcome, int exitCode = 0, bool producedOutput = true,
                  std::chrono::milliseconds elapsed = std::chrono::milliseconds(100)) {
        RunResult r;
        r.script = requests.back().script;
        r.outcome = outcome;
        r.exitCode = exitCode;
        r.producedOutput = producedOutput;
        r.elapsed = elapsed;
        if (outcome == RunOutcome::FailedToStart) {
            r.message = QStringLiteral("fake start failure");
        }
        m_running = false;
        emit finished(r);
    }

    std::vector<RunRequest> requests;
    int stopCalls = 0;

private:
    bool m_running = false;
};

class FakeLocator final : public IInterpreterLocator {
public:
    [[nodiscard]] InterpreterLookup locate(const QString& userOverride) const override {
        lastOverride = userOverride;
        return lookup;
    }
    InterpreterLookup lookup{QStringLiteral("C:/py/python.exe"), {}};
    mutable QString lastOverride;
};

}  // namespace pyrun::testing
