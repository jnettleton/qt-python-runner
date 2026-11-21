#pragma once

#include <cstdio>

namespace pyrun {

// Everything that decides whether one output stream gets ANSI colour. Pure data
// in, bool out, so the policy is testable without a console.
struct ColorPolicyInputs {
    bool noColorFlag = false;       // --no-color
    bool noColorEnv = false;        // NO_COLOR set and non-empty (no-color.org)
    bool interactive = false;       // the stream is a console/TTY, not a file or pipe
    bool vtAvailable = false;       // VT/ANSI processing is (or could be switched) on
};

[[nodiscard]] bool shouldColor(const ColorPolicyInputs& in);

// NO_COLOR present with a non-empty value.
[[nodiscard]] bool noColorEnvironmentSet();

// RAII, owned by main()'s scope. For each of stdout/stderr that is an
// interactive console and where colour is allowed, turns on VT processing and
// restores the previous console mode on destruction (the mode belongs to the
// shared console, so it must not outlive us). Redirected streams are never
// touched. Elsewhere (non-Windows) it asks isatty() and TERM.
class ConsoleColorGuard {
public:
    explicit ConsoleColorGuard(bool noColorFlag);
    ~ConsoleColorGuard();
    ConsoleColorGuard(const ConsoleColorGuard&) = delete;
    ConsoleColorGuard& operator=(const ConsoleColorGuard&) = delete;

    [[nodiscard]] bool stdoutColor() const { return m_stdoutColor; }
    [[nodiscard]] bool stderrColor() const { return m_stderrColor; }

private:
    bool m_stdoutColor = false;
    bool m_stderrColor = false;
    // Windows: console modes to restore. Stored as plain integers to keep
    // windows.h out of this header.
    unsigned long m_stdoutOriginalMode = 0;
    unsigned long m_stderrOriginalMode = 0;
    bool m_stdoutRestore = false;
    bool m_stderrRestore = false;
};

}  // namespace pyrun
