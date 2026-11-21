#include "ConsoleColor.h"

#include <QString>
#include <QtGlobal>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace pyrun {

bool shouldColor(const ColorPolicyInputs& in) {
    return !in.noColorFlag && !in.noColorEnv && in.interactive && in.vtAvailable;
}

bool noColorEnvironmentSet() {
    return !qEnvironmentVariable("NO_COLOR").isEmpty();
}

namespace {

#ifdef Q_OS_WIN
// Decides for one standard handle; on success may have switched on VT
// processing, in which case the mode to restore is returned through `restore`.
bool enableFor(DWORD stdHandleId, bool allowed, unsigned long& originalMode, bool& restore) {
    ColorPolicyInputs in;
    in.noColorFlag = !allowed;  // flag and env already folded into `allowed`
    HANDLE handle = GetStdHandle(stdHandleId);
    DWORD mode = 0;
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE ||
        GetFileType(handle) != FILE_TYPE_CHAR || !GetConsoleMode(handle, &mode)) {
        return false;  // redirected to a file or pipe: plain text
    }
    in.interactive = true;
    if (!allowed) {
        return shouldColor(in);
    }
    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
        in.vtAvailable = true;  // already on (Windows Terminal, modern conhost)
    } else if (SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
        in.vtAvailable = true;
        originalMode = mode;
        restore = true;
    }
    return shouldColor(in);
}
#endif

}  // namespace

ConsoleColorGuard::ConsoleColorGuard(bool noColorFlag) {
    const bool allowed = !noColorFlag && !noColorEnvironmentSet();
#ifdef Q_OS_WIN
    m_stdoutColor = enableFor(STD_OUTPUT_HANDLE, allowed, m_stdoutOriginalMode, m_stdoutRestore);
    m_stderrColor = enableFor(STD_ERROR_HANDLE, allowed, m_stderrOriginalMode, m_stderrRestore);
#else
    const bool termOk = qEnvironmentVariable("TERM") != QLatin1String("dumb");
    ColorPolicyInputs in;
    in.noColorFlag = noColorFlag;
    in.noColorEnv = noColorEnvironmentSet();
    in.vtAvailable = termOk;
    in.interactive = isatty(STDOUT_FILENO) != 0;
    m_stdoutColor = shouldColor(in);
    in.interactive = isatty(STDERR_FILENO) != 0;
    m_stderrColor = shouldColor(in);
#endif
}

ConsoleColorGuard::~ConsoleColorGuard() {
#ifdef Q_OS_WIN
    if (m_stdoutRestore) {
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), m_stdoutOriginalMode);
    }
    if (m_stderrRestore) {
        SetConsoleMode(GetStdHandle(STD_ERROR_HANDLE), m_stderrOriginalMode);
    }
#endif
}

}  // namespace pyrun
