#pragma once

#include "models/OutputChannel.h"
#include "models/StatusSeverity.h"

#include <QString>

namespace pyrun {

// Passive view. No decisions, no state beyond what it displays. User actions
// travel the other way: the concrete view exposes signals/callbacks that the
// composition root (main.cpp) wires to RunPresenter. This header must stay
// free of any QtWidgets/QtGui include.
class IRunView {
public:
    virtual ~IRunView() = default;

    // Script output (decoded), tagged with the stream it came from. May end
    // mid-line. Order is exact within a channel; across the two channels it is
    // the best-effort order in which the pipes were drained (see design.md).
    virtual void showOutput(const QString& text, OutputChannel channel) = 0;
    // Run headers, per-script results, summaries. Always whole lines, and the
    // presenter guarantees the output stream is at a line start before it.
    // The presenter picks the severity; the view only maps it to a look.
    virtual void showStatus(const QString& line, StatusSeverity severity) = 0;
    // A failure the user must see (no interpreter, nothing to run). Always
    // presented as a failure.
    virtual void showError(const QString& message) = 0;
    virtual void setRunning(bool running) = 0;
    virtual void clearOutput() = 0;
};

}  // namespace pyrun
