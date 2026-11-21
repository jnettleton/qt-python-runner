# Python Runner

A learning project by NCC. Two Qt 6 apps over one UI-agnostic core:

| App | Exe | What it does |
|-----|-----|--------------|
| Python Runner (GUI, Qt Widgets) | `pyrun-gui` (`.exe` on Windows) | Pick a `.py` file, Run, watch output live, Stop to kill it. Help > About shows name and version. |
| pyrun (console) | `pyrun` (`.exe` on Windows) | Runs `.py` files given on the command line and/or in a list file, one after another. |

Design and decisions: [docs/design.md](docs/design.md).

Layout: `core/{models,services,presenters,views}`, `gui/views`, `cli/views` (see the design note).

## Prerequisites

Windows (x64):

- Windows 10/11
- Visual Studio 2022 **or** 2026 with the "Desktop development with C++" workload
  (2022 is the default; see "Choosing the Visual Studio version")
- Qt 6.12 LTS, `msvc2022_64` kit (default expected at `C:\Qt\6.12.1\msvc2022_64`; CMake requires 6.5 or newer)
- CMake 3.25+ and Ninja on PATH
- Python 3 (only for the integration tests and for actually running scripts)

If Qt lives elsewhere, either set `QT_PREFIX` for `scripts\build.cmd`, or create an
untracked `CMakeUserPresets.json` that overrides `CMAKE_PREFIX_PATH`.

Linux (x86-64; written and verified on Ubuntu 26.04 / GCC 15 / distro Qt 6.10):

- A C++20 compiler (GCC 11+ or Clang 14+), CMake 3.25+, Ninja
- Qt 6.5 or newer with Core, Widgets and Test: the distro packages, or an
  official/`aqt` install passed as `QT_PREFIX`
- Python 3 (integration tests, and for running scripts)

Debian/Ubuntu packages (needs sudo; nothing here installs them for you):

    sudo apt install build-essential cmake ninja-build python3 \
        qt6-base-dev qt6-base-dev-tools libgl1-mesa-dev

Fedora: `sudo dnf install gcc-c++ cmake ninja-build python3 qt6-qtbase-devel`.
The GUI tests run with `QT_QPA_PLATFORM=offscreen` (set by CTest), so no display
or X server is needed to build and test.

## Build, test (Windows)

From an x64 Native Tools / Developer prompt:
    cmake --preset msvc-debug
    cmake --build --preset msvc-debug
    ctest --preset msvc-debug

Presets: `msvc-debug`, `msvc-release`, `msvc-asan` (AddressSanitizer on
RelWithDebInfo; MSVC has no UBSan/TSan). Warnings are errors (`/W4 /WX`); turn
off with `-DPYRUN_WARNINGS_AS_ERRORS=OFF`. `msvc-asan` builds the GUI with Qt's own
(non-native) file dialog; Debug and Release keep the native one. See `docs/design.md`.

From any shell (finds Visual Studio with vswhere and calls vcvars64 itself):

    scripts\build.cmd [msvc-debug|msvc-release|msvc-asan] [configure|build|test|all] [2022|2026|latest]

### Choosing the Visual Studio version

Qt's `msvc2022_64` kit is built and supported with VS 2022 (toolset v143), so
`build.cmd` uses **2022 by default**, even when 2026 is installed. MSVC keeps its
C++ ABI binary compatible across v140 to v145, so the same kit also links with the
VS 2026 toolset (v145, `cl` 19.5x). Pick 2026 explicitly with the third argument or
the environment:

    scripts\build.cmd msvc-debug all 2026
    set VSVERSION=2026 && scripts\build.cmd msvc-release

`VSVERSION` is `2022` (default), `2026` or `latest` (the newest installed).
`VSINSTALL=<install dir>` selects one exact installation (it wins over `VSVERSION`).
A 2026 build uses its own tree, `build\<preset>-vs2026`, so two compilers never
share a CMake cache; a 2022 build stays in `build\<preset>`. (If you drive CMake by
hand from a 2026 prompt, set `PYRUN_BUILD_TAG=-vs2026` first to get the same
separation, or use a fresh build directory.) CMake 4.1 has no Visual Studio 2026
generator, which is why the presets use Ninja.

Whole-program optimisation (`/GL`, `/LTCG`, `CMAKE_INTERPROCEDURAL_OPTIMIZATION`)
is the one thing that is *not* binary compatible between toolset versions, so
`cmake/PyrunToolchain.cmake` stops the configure if any of them is enabled.
`windeployqt` and the ASan runtime DLL copy follow whichever `cl.exe` the build
configured with.

## Build, test (Linux)

    cmake --preset linux-debug
    cmake --build --preset linux-debug
    ctest --preset linux-debug

Presets: `linux-debug`, `linux-release`, `linux-asan` (`-fsanitize=address,undefined`,
a finding aborts the test). Compiler: the system `c++` (GCC on Ubuntu); use
`CXX=clang++ scripts/build.sh ...` or `-DCMAKE_CXX_COMPILER=clang++` for Clang
(not tested). Warnings are errors (`-Wall -Wextra -Wpedantic -Werror`), same switch
as Windows.

Or the script, which mirrors `build.cmd`:

    scripts/build.sh [linux-debug|linux-release|linux-asan] [configure|build|test|all]

with `QT_PREFIX=~/Qt/6.12.1/gcc_64` for a Qt that is not the distro's, `CXX=` for
the compiler, and `BUILD_DIR=` to put the build tree elsewhere. Under WSL keep the
checkout (or at least `BUILD_DIR`) on the Linux filesystem: building on `/mnt/c` works
but is slow.

Integration tests (`tst_processrunner`) skip cleanly if no working Python is
found. Force one with `set PYRUN_TEST_PYTHON=C:\path\to\python.exe` (Linux:
`export PYRUN_TEST_PYTHON=/path/to/python3`). The process-tree tests skip when the
interpreter is a Windows Store/`WindowsApps` launcher alias, because the Job Object does
not capture the real interpreter behind it; set `PYRUN_TEST_PYTHON` to a real one.

## Run

    build\msvc-debug\gui\pyrun-gui.exe
    build\msvc-debug\cli\pyrun.exe --version

Linux: `build/linux-debug/gui/pyrun-gui`, `build/linux-debug/cli/pyrun`. The build
RPATH points at the Qt it was linked against, so nothing needs to be on
`LD_LIBRARY_PATH`.

On Windows each build runs `windeployqt` on the exe (POST_BUILD, see `cmake/PyrunDeploy.cmake`),
so the Qt DLLs and `platforms\qwindows(d).dll` sit next to it, and the asan preset
also copies the MSVC ASan runtime DLL. No PATH setup is needed:

- Explorer: double-click `build\<preset>\gui\pyrun-gui.exe`.
- Visual Studio 2022 (Open Folder): pick the preset, choose `pyrun-gui.exe` or
  `pyrun.exe` in "Select Startup Item", press F5.
- Command line: run the exe by path, from any shell.

Deployment re-runs only when the exe is relinked. The test exes (`tst_*.exe`) are not
deployed; ctest puts Qt's `bin` on PATH for them.

Try it by hand with `samples\hellopython.py` (prints a greeting, the Python version,
non-ASCII text, one stderr line, then a few lines half a second apart so live streaming
and Stop are visible; exits 0). It is not part of ctest.

    build\msvc-debug\cli\pyrun.exe samples\hellopython.py

### Console usage

    pyrun [options] [scripts...]

    -h, --help             Displays help on commandline options.
    -v, --version          Displays version information.
    -f, --file-list <path> Read script paths from <path>, one per line. Blank
                           lines and lines starting with '#' are ignored;
                           relative paths are relative to the list file. May be
                           repeated; runs after any scripts given on the
                           command line.
    --python <path>        Python interpreter to use (default: auto-detect).
    --timeout <seconds>    Kill a script that runs longer than <seconds>
                           (default: no timeout).
    --fail-fast            Stop at the first failed script.

    --no-color             Never colour output.

Script output (the script's stdout and stderr) goes to stdout. Run headers,
per-script results and the summary go to stderr, so `pyrun a.py > out.txt`
captures exactly the script's output.

The script's stderr text is shown in red when stdout is an interactive console, and
pyrun's own failure lines (`[failed]`, `[timeout]`, `[stopped]`, `FAILED` in the summary,
and errors such as "no interpreter") are red when stderr is. `[ok]` and info lines stay plain.
No escape codes are written when redirected to a file or pipe, when `NO_COLOR`
is set, or with `--no-color`. On Linux the test is `isatty()` on the stream plus
`TERM` not `dumb`; Windows additionally switches VT processing on for the console. In the GUI, stderr text and failure lines are red in the output pane (and failures in the status bar)
(`kStderrColor` in `gui/views/OutputFormats.h`; the lightness adapts to light and
dark themes).

Ordering: stdout and stderr are separate pipes. Order is exact within each, but
the interleaving between the two is best effort (output that arrives together is
ordered stdout first, so a script that alternates rapidly is grouped). See `docs/design.md`.

Exit codes: `0` all succeeded; `1` at least one failed (non-zero exit, crash,
could not start, timed out) or was not run because of `--fail-fast`; `2` usage
error or nothing could run (no scripts, no interpreter).

## Interpreter notes

- Auto-detect looks on PATH and runs the absolute path: `python.exe`, `python3.exe`,
  `py.exe` on Windows; `python3` then `python` (must be executable) on Linux.
- Windows: `.bat`/`.cmd` shims are refused on purpose (cmd.exe argument parsing). **pyenv-win
  users:** `python` on PATH is a `.bat` shim, so pass the real interpreter. Find it with
  `pyenv which python`, then `pyrun --python <that path> a.py` (or paste it into the GUI's
  Interpreter field). Linux has no such rule: a `pyenv`/`asdf` shim is an ordinary
  executable script started with an argument list, so it is accepted.
- The GUI remembers the interpreter path (only that, not the last script) in
  `%APPDATA%\NCC\Python Runner.ini` (Linux: `~/.config/NCC/Python Runner.ini`).
- Stop and `--timeout` kill the script's whole process tree, including children it
  spawned. Windows: a Job Object, killed at once. Linux: the interpreter runs in its
  own process group; the group gets SIGTERM, then SIGKILL after 2 seconds if the run
  has not finished. Children also die when the run ends; a script cannot leave background
  processes behind (a child that deliberately calls `setsid()` leaves the group and
  escapes on Linux).

## Version resources

Windows only: both exes carry a VERSIONINFO resource generated from the CMake
`project(VERSION)` (`cmake/version.rc.in`). There is no icon, by decision.

## Deploy

### Linux

    cmake --build --preset linux-release
    cmake --install build/linux-release --prefix ~/pyrun

This installs `bin/pyrun` and `bin/pyrun-gui` and nothing else: Qt is not bundled.
If the Qt you built against is outside the system library path (an official or `aqt`
install), the installed exes carry an RPATH to it. With distro Qt the target machine
needs the matching runtime packages (Debian/Ubuntu: `libqt6widgets6` and a platform
plugin; `libqt6core6` suffices for `pyrun`). Qt's own deploy script is not used on
Linux: tried against distro Qt 6.10 it copied about 200 libraries and then failed for
lack of `patchelf`; it is designed for an official Qt layout. A self-contained bundle
(AppImage via linuxdeploy's Qt plugin) is an open item.

### Windows

    cmake --build --preset msvc-release
    cmake --install build\msvc-release --prefix dist

The install step runs the Qt deploy script (windeployqt under the hood) for
both exes. Target machines need the Microsoft VC++ 2015-2022 x64 redistributable.

Clean-machine check: copy `dist\` somewhere outside the build tree, make sure
Qt is not on PATH, and run `pyrun.exe --version` and `pyrun-gui.exe`. The
classic failure is "could not find the Qt platform plugin 'windows'".

## Qt

This project is built with Qt 6 (open-source edition). The Qt modules it uses
(Core, Gui and Widgets; Test only in the test suite) are available under the
GNU LGPL v3; see <https://www.qt.io/licensing/> and
<https://doc.qt.io/qt-6/licensing.html>. Qt is linked dynamically and is not
part of this repository. Qt's source is available from <https://download.qt.io/>.
Qt is a trademark of The Qt Company Ltd.; this project is not affiliated with
or endorsed by it.

## License

The MIT License in [LICENSE](LICENSE) covers this project's own code only. It
does not apply to Qt or any other third-party component, which keep their own
licenses.
