# Design note

## Shape

    gui/  pyrun-gui   Qt Widgets view  --\
    cli/  pyrun       console view     ---+--> core/ runner_core (static lib, Qt6::Core only)
    tests/            Qt Test under CTest

Dependency rule, enforced by target links: `runner_core` links `Qt6::Core` and
nothing else; `gui_view` adds `Qt6::Widgets`; `pyrun` (CLI) never links Widgets
or Gui. A widget include in core fails to compile.

## Source layout

One class per header/source pair, files named after the class.

    core/                      runner_core (Qt6::Core only)
      models/                  RunResult, OutputChannel, StatusSeverity, FileList (value types, no behaviour tied to a UI)
      services/                IProcessRunner, ProcessRunner, ProcessTree (platform tree-kill), PythonLocator, OutputDecoder
      presenters/              RunPresenter (depends on view interfaces only)
      views/                   IRunView (the view interface; no widget types)
    gui/                       pyrun-gui + gui_view (Qt6::Widgets)
      views/                   MainView, AboutView, OutputFormats.h (the stderr colour)
    cli/                       pyrun (Qt6::Core only)
      views/                   ConsoleView (implements IRunView on stdout/stderr)
      ConsoleColor.*           colour policy + VT console-mode RAII (not a view)
      ConsoleEncodingGuard.*   Windows console code page / binary mode RAII (no-op on POSIX; not a view)
    tests/

Includes are rooted at each target's directory (`"presenters/RunPresenter.h"`,
`"views/MainView.h"`). View interfaces live in core so the presenter can
name them; concrete views live in the target that owns their UI toolkit, so
Widgets cannot leak into core.

## Interfaces (MVP, passive view)

- `IRunView` (core): `showOutput(text, OutputChannel)`, `showStatus(line, StatusSeverity)`, `showError`, `setRunning`,
  `clearOutput`. No decisions. User intent flows back through signals on the
  concrete view (`MainView::runRequested/stopRequested`), wired to the
  presenter in `main.cpp`. The interface has no signals so the CLI view needs no QObject.
- `IProcessRunner` (core): `start(RunRequest)`, `stop()`, signals
  `outputReceived(QString, OutputChannel)` and `finished(RunResult)`. One `finished` per
  `start`, including `FailedToStart`. `ProcessRunner` is the QProcess
  implementation; tests use a fake.
- `IInterpreterLocator` (core): `locate(override)` returns a path or an
  actionable error. `PythonLocator` is the real one.
- `RunPresenter` (core): the only presenter. A GUI run is a batch of one.
  Owns validation, sequencing, the continue/fail-fast policy, messages, the
  Store-stub hint, and the exit-code mapping (`BatchSummary::exitCode`).

## Who's the parent?

| Resource | Owner | Killed by | Told of death via |
|----------|-------|-----------|-------------------|
| `ProcessRunner` | `main()` stack | n/a | n/a |
| per-run `QProcess` | `ProcessRunner` (QObject parent), `deleteLater` on completion | `stop()`, timeout timer, or `~ProcessRunner` as last resort | `finished(RunResult)` |
| timeout `QTimer` | `ProcessRunner` (member, parent `this`) | stopped in `complete()` | n/a |
| `RunPresenter` | `main()` stack; holds non-owning refs to view/runner/locator | n/a | `batchFinished` |
| GUI child widgets | `MainView` via Qt parent tree | n/a | n/a |
| `AboutView` | stack, modal, parent `MainView` | n/a | n/a |
| per-run `ProcessTree` (Windows: Job Object, KILL_ON_JOB_CLOSE; POSIX: process group) | `ProcessRunner` (`unique_ptr`) | `stop()`/timeout: Windows `TerminateJobObject`; POSIX `killpg(SIGTERM)`, then `killpg(SIGKILL)` after the grace timer. On run completion or runner death: handle close / `killpg(SIGKILL)` | n/a |
| kill-grace `QTimer` (POSIX escalation) | `ProcessRunner` (member, parent `this`) | stopped in `complete()` | n/a |
| grandchildren spawned by a script | the job / the process group | same as above | not reported individually |
| GUI interpreter setting | `QSettings` (INI) on `main()`'s stack | n/a | n/a |

Tree kill sits behind `ProcessTree` (`core/services/ProcessTree.h/.cpp`), the one place with
platform `#ifdef`s for it; `ProcessRunner` and the presenter are platform-neutral. Interface:
`prepare(QProcess&)` before start, `adopt(pid)` once running, `requestStop()` (returns whether
a hard kill must follow), `kill()`, and the destructor (kills stragglers).

- **Windows.** Each run gets a Job Object. The interpreter is assigned on `QProcess::started`.
  Stop and timeout call `TerminateJobObject` (no graceful form) then `kill()`. The job is
  closed when the run completes, so leftover descendants die with it: a script cannot
  daemonise a child past its run (this also covers `py.exe`, which launches `python.exe`
  as a child). Known race: a descendant spawned between process creation and job
  assignment escapes; interpreter start-up makes this practically unreachable. If
  assignment fails, behaviour falls back to killing the direct child.
- **POSIX (Linux).** `prepare()` installs a `QProcess::setChildProcessModifier` that runs in
  the forked child before exec: `setpgid(0, 0)` (the child leads a new process group, so
  pgid == its pid and there is no assignment race) and, on Linux, `prctl(PR_SET_PDEATHSIG,
  SIGKILL)`. Stop and timeout send `SIGTERM` to the group and start a 2 s grace timer
  (`ProcessRunner::kKillGrace`); if the run has not completed when it fires, the group gets
  `SIGKILL` and the direct child `kill()`. Completing a run `SIGKILL`s whatever is left in
  the group. Why a group rather than `setsid`: same reach for our purposes, but the child
  keeps the session and controlling terminal. Why SIGTERM first: a script can clean up (the
  default Python disposition just dies; a handler can flush and exit).
  Costs and limits: a descendant that calls `setsid()`/`setpgid()` itself leaves the group
  and escapes; if pyrun itself is SIGKILLed or crashes, only the interpreter dies (via
  PDEATHSIG), not its group; the pgid can only be reused once the group is gone and reaped,
  so the final `killpg` has a theoretical pid-reuse window that needs a full pid wrap.
  What would change the decision: catching deliberate daemonisers needs cgroups or a
  subreaper, a much heavier design.

## Channels and encoding

- `QProcess::SeparateChannels` (changed from `MergedChannels` when nett asked for
  stderr highlighting). With one merged pipe the origin of a byte is gone at the
  OS level, so colouring is impossible after the fact. Each chunk is now tagged
  `OutputChannel::StdOut` or `StdErr` from `ProcessRunner` through the presenter
  to the view.
- **Ordering guarantee, stated plainly.** Within a channel, order is exact. Across
  the two channels it is *not*: stdout and stderr are two independent pipes, so
  there is no timestamp or sequence number to recover the interleaving. We drain
  best effort: a `readyRead` on either pipe schedules one drain through the
  event loop (a zero-delay single shot, no waiting), so the other pipe's reader
  can complete first; the drain then takes everything pending, stdout before
  stderr. This was measured, not assumed: reading the moment stderr signalled
  put a script's stderr line ahead of the stdout lines it wrote first, every
  run; the hop plus the stdout-first tie-break fixed `samples/hellopython.py`.
  The residual error: when both pipes hold data at once, all stdout precedes all
  stderr, so a script that alternates rapidly (`interleave.py`: out1 err1 out2
  err2 ...) comes out grouped (out1 out2 out3 err1 err2 err3), and
  stderr-then-stdout output that arrived together is reversed. Tests assert the per-channel streams only, never the
  cross-channel interleaving. The only way
  to get true order back is to merge again (losing the colour).
- One `OutputDecoder` per channel, each reset at run start and flushed at run
  end. A UTF-8 sequence or CRLF split across reads on one pipe cannot be
  disturbed by bytes on the other.
- The presenter's own repair newline (`ensureLineStart`) is tagged stdout, so it
  never carries stderr colour.
- Child gets `PYTHONUTF8=1`, `PYTHONUNBUFFERED=1` and `-u`; env starts from
  the system environment with those overrides; working directory is the
  script's directory; stdin is closed (script sees EOF).
- Decoding: `OutputDecoder` wraps a stateful UTF-8 `QStringDecoder`, one per
  run, so a multi-byte sequence split across reads survives. CRLF is
  normalised to LF with a held trailing CR so a split CRLF is not doubled.
  `fromLocal8Bit` is not used anywhere.
- PEP 686 (UTF-8 mode default in 3.15) not re-checked from the PEP page; the
  explicit env var covers older and newer Pythons either way.
- **stderr colour, GUI.** `MainView` inserts each chunk with an explicit
  `QTextCharFormat` from `gui/views/OutputFormats.h`. stdout uses the default
  format (palette-driven). stderr uses `kStderrColor`, a single hue constant;
  lightness is pinned from the output pane's palette (0.36 on a light base, 0.68
  on a dark base), so any hue stays readable. Yellow is a one-line change to
  `kStderrColor`.
- **stderr colour, CLI.** Script stderr text is still written to **stdout**
  (script output stays on stdout), wrapped in `ESC[91m ... ESC[0m` per chunk,
  only if stdout is an interactive console with VT processing. `ConsoleColorGuard`
  enables VT on a console that lacks it and restores the previous console mode at
  exit; redirected streams are never touched. Off when `--no-color` is given or
  `NO_COLOR` is set non-empty (no-color.org).
- **Failure status lines.** `IRunView::showStatus(line, StatusSeverity)` carries
  `Info` (run headers, `Summary:`), `Success` (`[ok]`, a clean totals line) or
  `Failure`. The presenter decides (`RunPresenter::severityOf`): anything other
  than a clean exit 0 is a failure, i.e. non-zero exit, crash, failed to start,
  `[stopped]`, `[timeout]`; the per-script `FAILED` summary rows; and the totals
  line when anything failed or was not run. `showError` (no interpreter, no
  scripts) is always a failure. Views never read the text.
  - GUI: failures use `stderrTextColor`, i.e. the same `kStderrColor` (no second
    constant), in the output pane and in the status bar (via its palette).
  - CLI: status lines go to stderr, so colour is decided per stream:
    `ConsoleColorGuard::stderrColor()` (stderr is an interactive VT console, not
    `NO_COLOR`/`--no-color`). Failure lines and errors are wrapped in
    `ESC[91m ... ESC[0m`; `[ok]` and info lines stay plain. stdout and stderr
    colour are independent: `pyrun a.py > out.txt` leaves the failure lines red
    on the console, while `2> err.txt` makes them plain.
  - No green for `[ok]`: not conventional for tools that print status, and an
    extra colour to keep legible on both themes for no information gain.
- CLI output: on Windows stdout/stderr are put in binary mode and the console code page
  switched to UTF-8 for the run and restored on exit (`ConsoleEncodingGuard`); on POSIX
  there is nothing to do (no newline translation; terminals take UTF-8 bytes). We write
  UTF-8 bytes, so a console and a redirect to a file produce the same bytes.
- CLI colour on POSIX: `isatty()` per stream, `TERM` not `dumb`, no `NO_COLOR`, no
  `--no-color`; no console-mode switching or restoring is involved.

## Interpreter resolution

Absolute path always. Auto-detect scans PATH directories one at a time with
`QStandardPaths::findExecutable` for `python.exe`, `python3.exe`, `py.exe` (Windows) or
`python3`, `python` (POSIX; the execute bit is required). The `.bat`/`.cmd` rule and the
`WindowsApps` fallback below are Windows-only (`kBatchRule` in `PythonLocator.cpp`): a POSIX
shim is a plain executable launched with an argv list, so there is no cmd.exe-style
re-parsing to guard against. On Windows `.bat`/`.cmd` are never chosen (cmd.exe argument parsing); when only a batch shim is found, the error names it and tells pyenv-win users to use `pyenv which python`. A hit under
`WindowsApps` is used only as a last resort; if the run then exits at once
with no output, the presenter says it is probably the Microsoft Store stub and
what to do. Override: `--python` / the GUI Interpreter field.

## Timeout policy

None by default. GUI: Stop button (and closing the window) kills the script.
CLI: `--timeout <seconds>` applies per script; at expiry the child is killed
and reported `[timeout]`; the batch then continues (or stops with `--fail-fast`).
`kill()` rather than `terminate()` on Windows because a console Python ignores WM_CLOSE;
on POSIX the stop is SIGTERM, escalating to SIGKILL after 2 s (see Tree kill).

## Exit-code policy (CLI)

`0` all scripts succeeded. `1` any script failed or was not run. `2` usage
error, unreadable file list, or nothing could run. Default is continue past
failures with a summary; `--fail-fast` stops at the first failure.

## Threads

None. QProcess is asynchronous; the CLI is event driven (the next script is
chained off `finished`, `batchFinished` quits the loop with the exit code).
No `waitFor*` on any GUI path (tests call them only on a test thread).

## Toolchains and platforms

- **MSVC.** Qt's kit is `msvc2022_64`; VS 2022 is the default toolset and VS 2026 (v145) is
  supported through MSVC's v140-v145 binary compatibility. The exception, whole-program
  optimisation (`/GL`, `/LTCG`), is refused at configure time (`cmake/PyrunToolchain.cmake`).
  `build.cmd` picks the VS generation (`VSVERSION`) and gives 2026 its own build tree via
  `PYRUN_BUILD_TAG` (`binaryDir` in `CMakePresets.json` reads `$env{PYRUN_BUILD_TAG}`).
  A parallel set of `msvc2026-*` presets was rejected: three more of everything for a
  property that is really "which `cl` is on PATH".
- **Linux.** GCC (Ubuntu 15.2) with Ninja; Clang is not tested. `linux-asan` is
  `-fsanitize=address,undefined -fno-sanitize-recover=all`; MSVC has no UBSan, so the Linux
  asan preset is the stronger one. Platform-specific code is confined to `ProcessTree.cpp`,
  the locator's platform constants, the `ConsoleColor`/`ConsoleEncodingGuard` guards (already
  `#ifdef`'d), and two strings in `MainView`. Windows-only CMake: `enable_language(RC)`,
  VERSIONINFO, `windeployqt`, the Qt deploy script, `PATH` prepending in tests.
- **Install on Linux** is `bin/` plus an RPATH; no bundling (see README, Deploy).
- **Line endings.** `.gitattributes` pins `*.sh` and `*.py` to LF (a CRLF shebang breaks) and
  `*.cmd` to CRLF.

## Not done / deliberate omissions

- Linux: no self-contained bundle (AppImage/linuxdeploy), no Clang or macOS verification.
- Linux: if pyrun is interrupted by Ctrl-C or SIGTERM it does not stop the running child
  group (only PDEATHSIG kills the interpreter). A signal handler that routes to `stop()` is
  the fix; not done.
- Remembering the last script (only the interpreter is remembered, by decision).
- No icon (by decision). VERSIONINFO is generated from project VERSION for both exes.
- No stdin forwarding to the script.
