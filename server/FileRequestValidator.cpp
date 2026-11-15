#include "server/FileRequestValidator.h"

#include <QDir>
#include <QFileInfo>

namespace fm {

namespace {

QString reject(QString* error, const QString& reason)
{
    if (error)
        *error = reason;
    return QString();
}

} // namespace

QString resolveRequestedFile(const QString& directory, const QString& fileName, QString* error)
{
    if (fileName.isEmpty())
        return reject(error, QStringLiteral("File name is empty"));

    const bool hasPathParts = fileName.contains(QLatin1Char('/'))
        || fileName.contains(QLatin1Char('\\'))
        || fileName.contains(QLatin1Char(':'))
        || fileName == QLatin1String(".")
        || fileName == QLatin1String("..");
    if (hasPathParts)
        return reject(error, QStringLiteral("File name must not contain a path"));

    const QFileInfo info(QDir(directory), fileName);
    if (!info.exists() || !info.isFile())
        return reject(error, QStringLiteral("File not found: %1").arg(fileName));

    const QString canonicalDir = QDir(directory).canonicalPath();
    const QString canonicalFile = info.canonicalFilePath();
    if (canonicalFile.isEmpty() || QFileInfo(canonicalFile).path() != canonicalDir)
        return reject(error, QStringLiteral("File is outside the watched directory"));

    return canonicalFile;
}

} // namespace fm
