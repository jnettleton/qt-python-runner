#include "models/FileList.h"

#include <QFile>
#include <QFileInfo>
#include <QStringDecoder>

namespace pyrun {

QStringList parseFileList(const QString& text, const QDir& baseDir) {
    QStringList paths;
    const auto lines = text.split(QLatin1Char('\n'));
    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();  // also drops a trailing '\r'
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (line.size() >= 2 && line.startsWith(QLatin1Char('"')) &&
            line.endsWith(QLatin1Char('"'))) {
            line = line.mid(1, line.size() - 2).trimmed();
            if (line.isEmpty()) {
                continue;
            }
        }
        paths.append(QDir::cleanPath(baseDir.absoluteFilePath(line)));
    }
    return paths;
}

FileListResult readFileList(const QString& listFilePath) {
    FileListResult result;
    const QFileInfo info(listFilePath);
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("Cannot read file list '%1': %2")
                           .arg(QDir::toNativeSeparators(listFilePath), file.errorString());
        return result;
    }
    QStringDecoder decoder(QStringConverter::Utf8);
    QString text = decoder(QByteArrayView(file.readAll()));
    if (text.startsWith(QChar(0xFEFF))) {
        text.remove(0, 1);
    }
    result.paths = parseFileList(text, info.absoluteDir());
    return result;
}

}  // namespace pyrun
