#include "views/MainView.h"
#include "services/ProcessRunner.h"
#include "services/PythonLocator.h"
#include "presenters/RunPresenter.h"
#include "pyrun/version.h"

#include <QApplication>
#include <QSettings>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QString::fromLatin1(pyrun::kGuiName));
    QApplication::setApplicationVersion(QString::fromLatin1(pyrun::kVersion));
    QApplication::setOrganizationName(QString::fromLatin1(pyrun::kOrganization));

    // Composition root. Everything lives on main()'s stack; reverse destruction
    // tears down presenter, window, locator, then runner (which kills any child).
    pyrun::ProcessRunner runner;
    const pyrun::PythonLocator locator;
    pyrun::MainView window;
    pyrun::RunPresenter presenter(window, runner, locator);

    // Remember the interpreter path only (not the last script). INI file in the
    // user's AppData/NCC folder, not the registry. Owner: this stack object.
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QString::fromLatin1(pyrun::kOrganization),
                       QString::fromLatin1(pyrun::kGuiName));
    window.setPythonPath(settings.value(QStringLiteral("interpreter")).toString());

    QObject::connect(&window, &pyrun::MainView::runRequested, &presenter,
                     [&presenter, &settings](const QString& script, const QString& python) {
                         settings.setValue(QStringLiteral("interpreter"), python);
                         pyrun::RunOptions options;
                         options.python = python;
                         presenter.run({script}, options);
                     });
    QObject::connect(&window, &pyrun::MainView::stopRequested, &presenter,
                     &pyrun::RunPresenter::stop);

    window.show();
    return QApplication::exec();
}
