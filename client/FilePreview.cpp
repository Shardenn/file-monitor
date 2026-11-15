#include "client/FilePreview.h"

#include <QFile>
#include <QTextCodec>

namespace fm {

FilePreview loadFilePreview(const QString& filePath, qint64 maxBytes)
{
    FilePreview preview;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return preview;

    preview.fileSize = file.size();
    const QByteArray data = file.read(maxBytes);
    preview.truncated = preview.fileSize > data.size();

    if (data.contains('\0'))
        return preview;

    // A multi-byte character cut off at the end of the preview shows up in
    // remainingChars, not invalidChars, so truncation alone does not make a
    // text file look binary.
    QTextCodec::ConverterState state;
    const QTextCodec* utf8 = QTextCodec::codecForName("UTF-8");
    const QString text = utf8->toUnicode(data.constData(), data.size(), &state);
    if (state.invalidChars > 0)
        return preview;

    preview.isText = true;
    preview.text = text;
    return preview;
}

} // namespace fm
