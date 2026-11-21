#pragma once

namespace pyrun {

// RAII: puts stdout/stderr into binary mode (so '\n' is not rewritten to
// "\r\n" by the CRT) and, on Windows, switches the console output code page
// to UTF-8 for the lifetime of the object, restoring it on exit. Redirected
// output (file/pipe) is raw UTF-8 either way.
class ConsoleEncodingGuard {
public:
    ConsoleEncodingGuard();
    ~ConsoleEncodingGuard();
    ConsoleEncodingGuard(const ConsoleEncodingGuard&) = delete;
    ConsoleEncodingGuard& operator=(const ConsoleEncodingGuard&) = delete;

private:
    unsigned m_previousCodePage = 0;
};

}  // namespace pyrun
