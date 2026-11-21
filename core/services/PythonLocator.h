#pragma once

#include <QString>
#include <QStringList>

namespace pyrun {

struct InterpreterLookup {
    QString path;   // absolute path to the interpreter executable
    QString error;  // user-facing, actionable; set when path is empty
    [[nodiscard]] bool ok() const { return !path.isEmpty(); }
};

class IInterpreterLocator {
public:
    virtual ~IInterpreterLocator() = default;
    // userOverride empty -> auto-detect.
    [[nodiscard]] virtual InterpreterLookup locate(const QString& userOverride) const = 0;
};

// Resolves to an ABSOLUTE path (a bare name would go through CreateProcess
// search order, which includes the app/current directory before PATH).
//  - Auto-detect scans PATH directories for python.exe, python3.exe, py.exe
//    via QStandardPaths::findExecutable, one directory at a time.
//  - .bat/.cmd are never chosen: launching them routes arguments through
//    cmd.exe parsing (quoting/injection risk). This deliberately skips the
//    pyenv-win shims; use --python / the Interpreter field for those.
//  - A hit under ...\WindowsApps\ may be the Microsoft Store stub; it is used
//    only if nothing better exists, and the presenter explains a silent
//    failure.
class PythonLocator final : public IInterpreterLocator {
public:
    // Default: directories from the PATH environment variable.
    PythonLocator();
    explicit PythonLocator(QStringList searchDirs);

    [[nodiscard]] InterpreterLookup locate(const QString& userOverride) const override;

    [[nodiscard]] static bool isStoreAlias(const QString& path);
    [[nodiscard]] static bool isBatchFile(const QString& path);

private:
    [[nodiscard]] InterpreterLookup resolveOverride(const QString& value) const;
    [[nodiscard]] InterpreterLookup autoDetect() const;

    QStringList m_dirs;
};

}  // namespace pyrun
