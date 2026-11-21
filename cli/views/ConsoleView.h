#pragma once

#include "views/IRunView.h"

#include <cstdio>

namespace pyrun {

// Console view. stdout carries ONLY the script output (so `pyrun a.py > out.txt`
// captures exactly that), both of the script's channels: the stderr text is
// written to stdout too, wrapped in an ANSI colour sequence when -- and only
// when -- colourStdout says stdout is an interactive VT console. Headers,
// results, summaries and errors go to stderr; failure lines and errors are red when
// colourStderr says stderr is an interactive VT console, [ok]/info lines are plain.
//
// The FILE* sinks are injectable so tests can capture the exact bytes.
class ConsoleView final : public IRunView {
public:
    // SGR 91 (bright red): stays legible on both dark and light terminals.
    // Switching the stderr colour in the CLI is this one constant.
    static constexpr const char* kStderrOn = "\x1b[91m";
    static constexpr const char* kReset = "\x1b[0m";

    // colourStdout / colourStderr: each stream is coloured only if it is an
    // interactive VT console (ConsoleColorGuard decides; NO_COLOR/--no-color off).
    explicit ConsoleView(bool colourStdout = false, bool colourStderr = false,
                         std::FILE* out = stdout, std::FILE* err = stderr)
        : m_colourStdout(colourStdout), m_colourStderr(colourStderr), m_out(out), m_err(err) {}

    void showOutput(const QString& text, OutputChannel channel) override;
    void showStatus(const QString& line, StatusSeverity severity) override;
    void showError(const QString& message) override;
    void setRunning(bool) override {}
    void clearOutput() override {}

private:
    static void write(std::FILE* stream, const QString& text);
    static QString red(const QString& text);

    bool m_colourStdout;
    bool m_colourStderr;
    std::FILE* m_out;
    std::FILE* m_err;
};

}  // namespace pyrun
