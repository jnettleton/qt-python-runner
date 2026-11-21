#include "views/ConsoleView.h"

#include "pyrun/version.h"

#include <QByteArray>
#include <QString>

namespace pyrun {

void ConsoleView::write(std::FILE* stream, const QString& text) {
    const QByteArray utf8 = text.toUtf8();
    std::fwrite(utf8.constData(), 1, static_cast<size_t>(utf8.size()), stream);
    std::fflush(stream);
}

QString ConsoleView::red(const QString& text) {
    return QString::fromLatin1(kStderrOn) + text + QString::fromLatin1(kReset);
}

void ConsoleView::showOutput(const QString& text, OutputChannel channel) {
    if (m_colourStdout && channel == OutputChannel::StdErr) {
        // Each chunk is self-contained (on, text, reset) so colour can never
        // bleed into the next chunk or past the end of the process.
        write(m_out, red(text));
        return;
    }
    write(m_out, text);
}

void ConsoleView::showStatus(const QString& line, StatusSeverity severity) {
    const QString text = line + QLatin1Char('\n');
    write(m_err, (m_colourStderr && severity == StatusSeverity::Failure) ? red(text) : text);
}

void ConsoleView::showError(const QString& message) {
    const QString text =
        QStringLiteral("%1: error: %2\n").arg(QString::fromLatin1(kCliName), message);
    write(m_err, m_colourStderr ? red(text) : text);
}

}  // namespace pyrun
