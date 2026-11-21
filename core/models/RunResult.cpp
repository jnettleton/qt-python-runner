#include "models/RunResult.h"

namespace pyrun {

QString formatElapsed(std::chrono::milliseconds elapsed) {
    return QString::number(static_cast<double>(elapsed.count()) / 1000.0, 'f', 2) +
           QStringLiteral(" s");
}

QString formatResult(const RunResult& r) {
    const QString time = formatElapsed(r.elapsed);
    QString line;
    switch (r.outcome) {
    case RunOutcome::Exited:
        line = QStringLiteral("[%1] exit code %2 (%3)")
                   .arg(r.exitCode == 0 ? QStringLiteral("ok") : QStringLiteral("failed"))
                   .arg(r.exitCode)
                   .arg(time);
        break;
    case RunOutcome::Crashed:
        line = QStringLiteral("[failed] process crashed, exit code %1 (%2)")
                   .arg(r.exitCode)
                   .arg(time);
        break;
    case RunOutcome::FailedToStart:
        line = QStringLiteral("[failed] could not run: %1").arg(r.message);
        return line;
    case RunOutcome::Stopped:
        line = QStringLiteral("[stopped] killed on request after %1").arg(time);
        break;
    case RunOutcome::TimedOut:
        line = QStringLiteral("[timeout] killed after %1").arg(time);
        break;
    }
    if (!r.message.isEmpty()) {
        line += QLatin1Char('\n') + r.message;
    }
    return line;
}

}  // namespace pyrun
