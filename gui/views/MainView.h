#pragma once

#include "views/IRunView.h"

#include <QMainWindow>
#include <QTextCharFormat>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace pyrun {

// Passive view. Slots forward and nothing else. User intent leaves through
// runRequested/stopRequested; main() connects them to RunPresenter.
//
// Ownership: all child widgets are parented (directly or through layouts) to
// this window; nothing is held in a smart pointer. The window itself lives on
// main()'s stack.
class MainView final : public QMainWindow, public IRunView {
    Q_OBJECT
public:
    explicit MainView(QWidget* parent = nullptr);

    // Seeds the Interpreter field (restored by main() from settings).
    void setPythonPath(const QString& path);

    // IRunView
    void showOutput(const QString& text, OutputChannel channel) override;
    void showStatus(const QString& line, StatusSeverity severity) override;
    void showError(const QString& message) override;
    void setRunning(bool running) override;
    void clearOutput() override;

signals:
    void runRequested(const QString& scriptPath, const QString& pythonPath);
    void stopRequested();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void browseForScript();
    void browseForPython();
    void appendText(const QString& text, const QTextCharFormat& format);
    void showInStatusBar(const QString& message, StatusSeverity severity);

    QLineEdit* m_scriptEdit = nullptr;
    QLineEdit* m_pythonEdit = nullptr;
    QPushButton* m_browseScript = nullptr;
    QPushButton* m_browsePython = nullptr;
    QPushButton* m_runButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QPushButton* m_clearButton = nullptr;
    QPlainTextEdit* m_output = nullptr;
};

}  // namespace pyrun
