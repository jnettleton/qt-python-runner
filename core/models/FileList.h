#pragma once

#include <QDir>
#include <QString>
#include <QStringList>

namespace pyrun {

// File-list format (one script path per line):
//   - UTF-8, optional BOM, LF or CRLF.
//   - Blank lines are skipped.
//   - A line whose first non-blank character is '#' is a comment. There are no
//     trailing comments ('#' is legal in a file name).
//   - Surrounding whitespace is trimmed; one pair of surrounding double quotes
//     is removed.
//   - Relative paths resolve against the directory of the list file.
[[nodiscard]] QStringList parseFileList(const QString& text, const QDir& baseDir);

struct FileListResult {
    QStringList paths;
    QString error;  // non-empty on failure
    [[nodiscard]] bool ok() const { return error.isEmpty(); }
};

[[nodiscard]] FileListResult readFileList(const QString& listFilePath);

}  // namespace pyrun
