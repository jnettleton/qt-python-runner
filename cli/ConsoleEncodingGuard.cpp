#include "ConsoleEncodingGuard.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <fcntl.h>
#include <io.h>

#include <cstdio>
#endif

namespace pyrun {

ConsoleEncodingGuard::ConsoleEncodingGuard() {
#ifdef Q_OS_WIN
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
    m_previousCodePage = GetConsoleOutputCP();  // 0 when there is no console
    if (m_previousCodePage != 0) {
        SetConsoleOutputCP(CP_UTF8);
    }
#endif
}

ConsoleEncodingGuard::~ConsoleEncodingGuard() {
#ifdef Q_OS_WIN
    if (m_previousCodePage != 0) {
        SetConsoleOutputCP(m_previousCodePage);
    }
#endif
}

}  // namespace pyrun
