#include "views/AboutView.h"

#include "pyrun/version.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace pyrun {

AboutView::AboutView(QWidget* parent) : QDialog(parent) {
    setObjectName(QStringLiteral("aboutDialog"));
    setWindowTitle(tr("About %1").arg(QString::fromLatin1(kGuiName)));

    auto* name = new QLabel(QString::fromLatin1(kGuiName), this);
    name->setObjectName(QStringLiteral("aboutName"));
    QFont f = name->font();
    f.setPointSizeF(f.pointSizeF() * 1.5);
    f.setBold(true);
    name->setFont(f);

    auto* version = new QLabel(tr("Version %1").arg(QString::fromLatin1(kVersion)), this);
    version->setObjectName(QStringLiteral("aboutVersion"));

    auto* detail = new QLabel(
        tr("Runs a Python script and shows its output live.\n"
           "A learning project by %1.")
            .arg(QString::fromLatin1(kOrganization)),
        this);

    // Qt attribution. Runtime version, so it reflects the Qt actually loaded.
    auto* qtNotice = new QLabel(
        tr("Built with Qt %1, used under the GNU LGPL v3. "
           "<a href=\"https://www.qt.io/licensing/\">Qt licensing</a>")
            .arg(QString::fromLatin1(qVersion())),
        this);
    qtNotice->setObjectName(QStringLiteral("aboutQtNotice"));
    qtNotice->setTextFormat(Qt::RichText);
    qtNotice->setOpenExternalLinks(true);
    qtNotice->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* aboutQt = buttons->addButton(tr("About Qt"), QDialogButtonBox::HelpRole);
    aboutQt->setObjectName(QStringLiteral("aboutQtButton"));
    // Forwards only: Qt's own dialog does the work. Context object: this.
    connect(aboutQt, &QPushButton::clicked, this, [this] { QMessageBox::aboutQt(this); });

    auto* layout = new QVBoxLayout(this);  // parent: this; owns the children above
    layout->addWidget(name);
    layout->addWidget(version);
    layout->addWidget(detail);
    layout->addWidget(qtNotice);
    layout->addWidget(buttons);
}

}  // namespace pyrun
