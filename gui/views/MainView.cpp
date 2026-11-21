#include "views/MainView.h"

#include "views/AboutView.h"
#include "views/OutputFormats.h"
#include "pyrun/version.h"

#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTextCursor>
#include <QVBoxLayout>

namespace pyrun {

MainView::MainView(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QString::fromLatin1(kGuiName));
    resize(900, 600);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    // Script row
    m_scriptEdit = new QLineEdit(central);
    m_scriptEdit->setObjectName(QStringLiteral("scriptEdit"));
    m_scriptEdit->setPlaceholderText(tr("Path to a .py file"));
    m_browseScript = new QPushButton(tr("Browse..."), central);
    m_browseScript->setObjectName(QStringLiteral("browseScriptButton"));
    auto* scriptRow = new QHBoxLayout;
    scriptRow->addWidget(new QLabel(tr("Script:"), central));
    scriptRow->addWidget(m_scriptEdit, 1);
    scriptRow->addWidget(m_browseScript);

    // Interpreter row
    m_pythonEdit = new QLineEdit(central);
    m_pythonEdit->setObjectName(QStringLiteral("pythonEdit"));
    m_pythonEdit->setPlaceholderText(tr("Auto-detect from PATH (or choose python.exe)"));
    m_browsePython = new QPushButton(tr("Browse..."), central);
    m_browsePython->setObjectName(QStringLiteral("browsePythonButton"));
    auto* pythonRow = new QHBoxLayout;
    pythonRow->addWidget(new QLabel(tr("Interpreter:"), central));
    pythonRow->addWidget(m_pythonEdit, 1);
    pythonRow->addWidget(m_browsePython);

    // Action row
    m_runButton = new QPushButton(tr("Run"), central);
    m_runButton->setObjectName(QStringLiteral("runButton"));
    m_runButton->setDefault(true);
    m_stopButton = new QPushButton(tr("Stop"), central);
    m_stopButton->setObjectName(QStringLiteral("stopButton"));
    m_stopButton->setEnabled(false);
    m_clearButton = new QPushButton(tr("Clear"), central);
    m_clearButton->setObjectName(QStringLiteral("clearButton"));
    auto* actionRow = new QHBoxLayout;
    actionRow->addWidget(m_runButton);
    actionRow->addWidget(m_stopButton);
    actionRow->addWidget(m_clearButton);
    actionRow->addStretch(1);

    // Output
    m_output = new QPlainTextEdit(central);
    m_output->setObjectName(QStringLiteral("outputView"));
    m_output->setReadOnly(true);
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_output->setMaximumBlockCount(200000);  // bound memory on runaway scripts

    root->addLayout(scriptRow);
    root->addLayout(pythonRow);
    root->addLayout(actionRow);
    root->addWidget(m_output, 1);
    setCentralWidget(central);

    // Help > About
    auto* helpMenu = menuBar()->addMenu(tr("&Help"));
    auto* aboutAction = helpMenu->addAction(tr("&About %1").arg(QString::fromLatin1(kGuiName)));
    aboutAction->setObjectName(QStringLiteral("aboutAction"));

    // Wiring: every slot forwards, nothing decides.
    connect(m_browseScript, &QPushButton::clicked, this, &MainView::browseForScript);
    connect(m_browsePython, &QPushButton::clicked, this, &MainView::browseForPython);
    connect(m_runButton, &QPushButton::clicked, this, [this] {
        emit runRequested(m_scriptEdit->text().trimmed(), m_pythonEdit->text().trimmed());
    });
    connect(m_scriptEdit, &QLineEdit::returnPressed, m_runButton, &QPushButton::click);
    connect(m_stopButton, &QPushButton::clicked, this, &MainView::stopRequested);
    connect(m_clearButton, &QPushButton::clicked, m_output, &QPlainTextEdit::clear);
    connect(aboutAction, &QAction::triggered, this, [this] {
        AboutView dialog(this);  // modal, stack-owned for its short life
        dialog.exec();
    });
}

void MainView::setPythonPath(const QString& path) {
    m_pythonEdit->setText(path);
}

void MainView::appendText(const QString& text, const QTextCharFormat& format) {
    // Always insert with an explicit format: the text edit would otherwise
    // inherit the previous run's format and stderr red would bleed forward.
    QTextCursor cursor(m_output->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text, format);
    m_output->moveCursor(QTextCursor::End);  // follow the tail
}

void MainView::showOutput(const QString& text, OutputChannel channel) {
    appendText(text, formatFor(channel, m_output->palette()));
}

void MainView::showInStatusBar(const QString& message, StatusSeverity severity) {
    // Palette, not a stylesheet: the colour then comes from the same helper as
    // the output pane. An empty palette restores the style's default.
    QPalette palette;
    if (severity == StatusSeverity::Failure) {
        palette.setColor(QPalette::WindowText, stderrTextColor(m_output->palette()));
    }
    statusBar()->setPalette(palette);
    statusBar()->showMessage(message.section(QLatin1Char('\n'), 0, 0));
}

void MainView::showStatus(const QString& line, StatusSeverity severity) {
    appendText(line + QLatin1Char('\n'), statusFormat(severity, m_output->palette()));
    showInStatusBar(line, severity);
}

void MainView::showError(const QString& message) {
    appendText(tr("error: %1\n").arg(message),
               statusFormat(StatusSeverity::Failure, m_output->palette()));
    showInStatusBar(message, StatusSeverity::Failure);
}

void MainView::setRunning(bool running) {
    m_runButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
    m_scriptEdit->setEnabled(!running);
    m_pythonEdit->setEnabled(!running);
    m_browseScript->setEnabled(!running);
    m_browsePython->setEnabled(!running);
}

void MainView::clearOutput() {
    m_output->clear();
}

void MainView::closeEvent(QCloseEvent* event) {
    emit stopRequested();  // never leave a child process behind
    QMainWindow::closeEvent(event);
}

void MainView::browseForScript() {
    const QString start = QFileInfo(m_scriptEdit->text()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select a Python script"), start,
        tr("Python files (*.py);;All files (*)"));
    if (!path.isEmpty()) {
        m_scriptEdit->setText(path);
    }
}

void MainView::browseForPython() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select a Python interpreter"), m_pythonEdit->text(),
        tr("Executables (*.exe);;All files (*)"));
    if (!path.isEmpty()) {
        m_pythonEdit->setText(path);
    }
}

}  // namespace pyrun
