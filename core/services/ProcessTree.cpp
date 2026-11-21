#include "services/ProcessTree.h"

#include <QProcess>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#include <sys/types.h>
#include <unistd.h>
#ifdef Q_OS_LINUX
#include <sys/prctl.h>
#endif
#endif

namespace pyrun {

#ifdef Q_OS_WIN

ProcessTree::ProcessTree() {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job == nullptr) {
        return;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        CloseHandle(job);
        return;
    }
    m_handle = job;
}

ProcessTree::~ProcessTree() {
    if (m_handle != nullptr) {
        CloseHandle(static_cast<HANDLE>(m_handle));  // KILL_ON_JOB_CLOSE reaps the tree
    }
}

void ProcessTree::prepare(QProcess&) {}

bool ProcessTree::adopt(qint64 pid) {
    if (m_handle == nullptr) {
        return false;
    }
    HANDLE process =
        OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (process == nullptr) {
        return false;
    }
    const BOOL ok = AssignProcessToJobObject(static_cast<HANDLE>(m_handle), process);
    CloseHandle(process);
    if (ok != FALSE) {
        m_pgid = pid;  // only the "adopted" marker on Windows
    }
    return ok != FALSE;
}

bool ProcessTree::adopted() const {
    return m_pgid != 0;
}

bool ProcessTree::requestStop() {
    kill();  // a Job Object has no graceful form
    return false;
}

void ProcessTree::kill() {
    if (m_handle != nullptr) {
        TerminateJobObject(static_cast<HANDLE>(m_handle), 1);
    }
}

#else  // POSIX

ProcessTree::ProcessTree() = default;

ProcessTree::~ProcessTree() {
    kill();  // stragglers of a finished run; ESRCH if the group is already empty
}

void ProcessTree::prepare(QProcess& process) {
    // Runs in the forked child before exec: async-signal-safe calls only.
    process.setChildProcessModifier([] {
        ::setpgid(0, 0);  // new group, led by the child: pgid == its pid
#ifdef Q_OS_LINUX
        // If we die without cleaning up (crash, SIGKILL, Ctrl-C), take the
        // interpreter with us. Covers the direct child only, not its group.
        ::prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif
    });
}

bool ProcessTree::adopt(qint64 pid) {
    if (pid <= 1) {  // never 0 (our own group) or 1 (init)
        return false;
    }
    m_pgid = pid;
    return true;
}

bool ProcessTree::adopted() const {
    return m_pgid != 0;
}

bool ProcessTree::requestStop() {
    if (m_pgid == 0) {
        return false;
    }
    return ::killpg(static_cast<pid_t>(m_pgid), SIGTERM) == 0;
}

void ProcessTree::kill() {
    if (m_pgid != 0) {
        ::killpg(static_cast<pid_t>(m_pgid), SIGKILL);
    }
}

#endif

}  // namespace pyrun
