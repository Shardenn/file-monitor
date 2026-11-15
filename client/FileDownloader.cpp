#include "client/FileDownloader.h"

namespace fm {

FileDownloader::FileDownloader(QObject* parent)
    : QObject(parent)
{
}

void FileDownloader::begin(quint64 requestId, const QString& fileName)
{
    abort(QStringLiteral("Superseded by a new download"));

    auto file = std::make_shared<QTemporaryFile>();
    if (!file->open()) {
        emit failed(fileName,
                    QStringLiteral("Cannot create a temporary file: %1").arg(file->errorString()));
        return;
    }

    m_file = std::move(file);
    m_fileName = fileName;
    m_requestId = requestId;
    m_received = 0;
}

void FileDownloader::handleChunk(const pb::FileChunk& chunk)
{
    if (!isActive() || chunk.request_id() != m_requestId)
        return;

    const qint64 total = static_cast<qint64>(chunk.total_size());
    const qint64 size = static_cast<qint64>(chunk.data().size());

    // TCP keeps the order, so a gap means the server and we disagree about
    // the transfer. Better to fail loudly than to show a corrupt file.
    if (static_cast<qint64>(chunk.offset()) != m_received || m_received + size > total) {
        abort(QStringLiteral("Received an unexpected part of the file"));
        return;
    }

    if (m_file->write(chunk.data().data(), size) != size) {
        abort(QStringLiteral("Cannot write the temporary file: %1").arg(m_file->errorString()));
        return;
    }
    m_received += size;
    emit progress(m_received, total);

    if (!chunk.is_last())
        return;

    if (m_received != total) {
        abort(QStringLiteral("The file is incomplete"));
        return;
    }

    m_file->close();
    const std::shared_ptr<QTemporaryFile> file = std::move(m_file);
    m_file.reset();
    emit finished(m_fileName, file);
}

void FileDownloader::handleCommandResult(const pb::CommandResult& result)
{
    if (isActive() && result.request_id() == m_requestId && !result.success())
        abort(QString::fromStdString(result.message()));
}

void FileDownloader::abort(const QString& reason)
{
    if (!isActive())
        return;
    m_file.reset();
    emit failed(m_fileName, reason);
}

} // namespace fm
