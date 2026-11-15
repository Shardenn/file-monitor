#include <gtest/gtest.h>

#include "TestUtils.h"
#include "client/FileDownloader.h"

namespace pb = fm::pb;

namespace {

pb::FileChunk makeChunk(quint64 requestId, quint64 total, quint64 offset, const std::string& data,
                        bool isLast)
{
    pb::FileChunk chunk;
    chunk.set_request_id(requestId);
    chunk.set_file_name("f.bin");
    chunk.set_total_size(total);
    chunk.set_offset(offset);
    chunk.set_data(data);
    chunk.set_is_last(isLast);
    return chunk;
}

// Records what the downloader reports.
struct Outcome
{
    explicit Outcome(fm::FileDownloader& downloader)
    {
        QObject::connect(&downloader, &fm::FileDownloader::finished,
                         [this](const QString&, std::shared_ptr<QTemporaryFile> file) {
                             ++finished;
                             QFile reader(file->fileName());
                             if (reader.open(QIODevice::ReadOnly))
                                 content = reader.readAll();
                         });
        QObject::connect(&downloader, &fm::FileDownloader::failed,
                         [this](const QString&, const QString& reason) {
                             ++failed;
                             error = reason;
                         });
        QObject::connect(&downloader, &fm::FileDownloader::progress,
                         [this](qint64 received, qint64) { lastProgress = received; });
    }

    int finished = 0;
    int failed = 0;
    QByteArray content;
    QString error;
    qint64 lastProgress = -1;
};

} // namespace

TEST(FileDownloader, ReassemblesChunksInOrder)
{
    fm::FileDownloader downloader;
    Outcome outcome(downloader);

    downloader.begin(7, "f.bin");
    ASSERT_TRUE(downloader.isActive());
    downloader.handleChunk(makeChunk(7, 10, 0, "0123", false));
    downloader.handleChunk(makeChunk(7, 10, 4, "4567", false));
    EXPECT_EQ(outcome.lastProgress, 8);
    EXPECT_EQ(outcome.finished, 0);
    downloader.handleChunk(makeChunk(7, 10, 8, "89", true));

    EXPECT_EQ(outcome.finished, 1);
    EXPECT_EQ(outcome.failed, 0);
    EXPECT_EQ(outcome.content, QByteArray("0123456789"));
    EXPECT_FALSE(downloader.isActive());
}

TEST(FileDownloader, HandlesEmptyFile)
{
    fm::FileDownloader downloader;
    Outcome outcome(downloader);

    downloader.begin(1, "empty");
    downloader.handleChunk(makeChunk(1, 0, 0, "", true));

    EXPECT_EQ(outcome.finished, 1);
    EXPECT_TRUE(outcome.content.isEmpty());
}

TEST(FileDownloader, KeepsBinaryDataIntact)
{
    fm::FileDownloader downloader;
    Outcome outcome(downloader);
    const std::string data("\0\x01\x02\xff\0end", 8);

    downloader.begin(1, "f.bin");
    downloader.handleChunk(makeChunk(1, data.size(), 0, data, true));

    EXPECT_EQ(outcome.content, QByteArray(data.data(), static_cast<int>(data.size())));
}

TEST(FileDownloader, IgnoresChunksOfOtherRequests)
{
    fm::FileDownloader downloader;
    Outcome outcome(downloader);

    downloader.handleChunk(makeChunk(1, 4, 0, "abcd", true)); // nothing active
    downloader.begin(2, "f.bin");
    downloader.handleChunk(makeChunk(1, 4, 0, "abcd", true)); // stale request

    EXPECT_EQ(outcome.finished, 0);
    EXPECT_EQ(outcome.failed, 0);
    EXPECT_TRUE(downloader.isActive());
}

TEST(FileDownloader, FailsOnGapInOffsets)
{
    fm::FileDownloader downloader;
    Outcome outcome(downloader);

    downloader.begin(1, "f.bin");
    downloader.handleChunk(makeChunk(1, 10, 0, "0123", false));
    downloader.handleChunk(makeChunk(1, 10, 6, "6789", true));

    EXPECT_EQ(outcome.finished, 0);
    EXPECT_EQ(outcome.failed, 1);
    EXPECT_FALSE(downloader.isActive());
}

TEST(FileDownloader, FailsWhenLastChunkLeavesFileIncomplete)
{
    fm::FileDownloader downloader;
    Outcome outcome(downloader);

    downloader.begin(1, "f.bin");
    downloader.handleChunk(makeChunk(1, 10, 0, "0123", true));

    EXPECT_EQ(outcome.finished, 0);
    EXPECT_EQ(outcome.failed, 1);
}

TEST(FileDownloader, FailsWhenDataExceedsDeclaredSize)
{
    fm::FileDownloader downloader;
    Outcome outcome(downloader);

    downloader.begin(1, "f.bin");
    downloader.handleChunk(makeChunk(1, 2, 0, "0123", true));

    EXPECT_EQ(outcome.failed, 1);
}

TEST(FileDownloader, FailedCommandResultAbortsOnlyItsOwnRequest)
{
    fm::FileDownloader downloader;
    Outcome outcome(downloader);
    downloader.begin(5, "f.bin");

    pb::CommandResult other;
    other.set_request_id(4);
    other.set_success(false);
    downloader.handleCommandResult(other);
    EXPECT_TRUE(downloader.isActive());

    pb::CommandResult ok;
    ok.set_request_id(5);
    ok.set_success(true);
    downloader.handleCommandResult(ok);
    EXPECT_TRUE(downloader.isActive());

    pb::CommandResult refused;
    refused.set_request_id(5);
    refused.set_success(false);
    refused.set_message("File not found");
    downloader.handleCommandResult(refused);

    EXPECT_FALSE(downloader.isActive());
    EXPECT_EQ(outcome.failed, 1);
    EXPECT_EQ(outcome.error, QString("File not found"));
}

TEST(FileDownloader, AbortWithoutDownloadDoesNothing)
{
    fm::FileDownloader downloader;
    Outcome outcome(downloader);
    downloader.abort("no reason");
    EXPECT_EQ(outcome.failed, 0);
}
