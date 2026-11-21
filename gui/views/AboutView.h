#pragma once

#include <QDialog>

namespace pyrun {

// Static information only; no logic. Name and version come from the
// generated version header, the Qt version from the runtime.
// Owner: the QWidget parent passed in (MainView), which deletes it.
class AboutView : public QDialog {
    Q_OBJECT
public:
    explicit AboutView(QWidget* parent = nullptr);
};

}  // namespace pyrun
