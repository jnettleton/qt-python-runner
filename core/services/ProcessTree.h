#pragma once

#include <QtGlobal>

class QProcess;

namespace pyrun {

// Everything needed to take down a child process AND the processes it spawned.
// Platform code lives here (and in ProcessTree.cpp), behind ProcessRunner, so
// neither the presenter nor the runner's logic knows which OS it is on.
//
// Ownership ("who's the parent?"): one instance per run, owned by
// ProcessRunner (unique_ptr), destroyed when the run completes. Destroying it
// kills any descendants still alive: a script's children never outlive its run.
//
//  - Windows: a Job Object with KILL_ON_JOB_CLOSE. The interpreter is
//    assigned after it starts; kill is TerminateJobObject (no graceful form).
//    Known race: a descendant spawned before the assignment escapes; the
//    interpreter's start-up time makes that practically unreachable.
//  - POSIX: the child leads its own process group (setpgid(0,0) in the forked
//    child, before exec; on Linux also PR_SET_PDEATHSIG=SIGKILL so the
//    interpreter dies with us). Stop is killpg(SIGTERM), kill is
//    killpg(SIGKILL). A grandchild that calls setsid()/setpgid() itself leaves
//    the group and escapes. Using a group, not a pid, also reaches children of
//    children. pgid == the interpreter's pid, which is only reusable after the
//    whole group is gone and reaped.
class ProcessTree {
public:
    ProcessTree();
    ~ProcessTree();
    ProcessTree(const ProcessTree&) = delete;
    ProcessTree& operator=(const ProcessTree&) = delete;

    // Call before QProcess::start(). POSIX: arranges for the child to lead a
    // new process group. Windows: nothing to do.
    static void prepare(QProcess& process);

    // Call once the child is running. Best effort; false leaves the runner's
    // plain QProcess::kill() of the direct child as the fallback.
    bool adopt(qint64 pid);
    [[nodiscard]] bool adopted() const;

    // Asks the whole tree to stop. Returns true if the request is graceful
    // (POSIX SIGTERM): the caller must escalate with kill() if the child is
    // still alive after a grace period. Returns false if the tree was already
    // killed hard, or could not be signalled (the caller then kills the direct
    // child itself).
    [[nodiscard]] bool requestStop();

    // Hard-kills the whole tree. Safe to call repeatedly and on a dead tree.
    void kill();

private:
    void* m_handle = nullptr;  // Windows: job HANDLE
    qint64 m_pgid = 0;         // POSIX: process group id; Windows: adopted marker
};

}  // namespace pyrun
