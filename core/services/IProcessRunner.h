#pragma once

#include "models/OutputChannel.h"
#include "models/RunResult.h"

#include <QObject>
#include <QString>

#include <chrono>

namespace pyrun {

struct RunRequest {
    QString interpreter;                 // absolute path, already resolved
    QString script;                      // absolute path to the .py file
    std::chrono::milliseconds timeout{0};  // 0 = no timeout
};

// Service seam over QProcess. One run at a time. Presenter tests use a fake;
// ProcessRunner is the real thing.
//
// Contract:
//  - start() returns immediately. Exactly one finished() per start(), always,
//    including FailedToStart.
//  - outputReceived() carries decoded text tagged with its channel, zero or
//    more times before finished(). Each channel has its own decoder state and
//    is exactly ordered; stdout and stderr are separate pipes, so their
//    relative order is approximate (drain order).
//  - stop() is a no-op when idle; otherwise the run ends with Stopped.
class IProcessRunner : public QObject {
    Q_OBJECT
public:
    explicit IProcessRunner(QObject* parent = nullptr) : QObject(parent) {}

    virtual void start(const RunRequest& request) = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool isRunning() const = 0;

signals:
    void outputReceived(const QString& text, pyrun::OutputChannel channel);
    void finished(const pyrun::RunResult& result);
};

}  // namespace pyrun
