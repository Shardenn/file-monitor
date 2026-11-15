#pragma once

#include <QDialog>

#include "client/FilePreview.h"

namespace fm {

// Shows a downloaded file: the text for text files, or a "no preview"
// message for binary ones.
class FilePreviewDialog : public QDialog
{
    Q_OBJECT

public:
    FilePreviewDialog(const QString& fileName, const FilePreview& preview,
                      QWidget* parent = nullptr);
};

} // namespace fm
