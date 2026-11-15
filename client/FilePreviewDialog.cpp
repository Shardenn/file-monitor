#include "client/FilePreviewDialog.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "client/EventRecord.h"

namespace fm {

FilePreviewDialog::FilePreviewDialog(const QString& fileName, const FilePreview& preview,
                                     QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("%1 (%2)").arg(fileName, formatFileSize(
                                                               static_cast<quint64>(preview.fileSize))));
    resize(720, 520);

    auto* layout = new QVBoxLayout(this);

    if (preview.isText) {
        auto* viewer = new QPlainTextEdit(this);
        viewer->setReadOnly(true);
        viewer->setLineWrapMode(QPlainTextEdit::NoWrap);
        viewer->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        viewer->setPlainText(preview.text);
        layout->addWidget(viewer);

        if (preview.truncated) {
            layout->addWidget(new QLabel(
                QStringLiteral("Only the first %1 of the file are shown.")
                    .arg(formatFileSize(static_cast<quint64>(kPreviewLimitBytes))),
                this));
        }
    } else {
        auto* message = new QLabel(QStringLiteral("No preview available (binary file)."), this);
        message->setAlignment(Qt::AlignCenter);
        layout->addWidget(message, 1);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

} // namespace fm
