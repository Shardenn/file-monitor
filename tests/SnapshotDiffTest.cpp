#include <gtest/gtest.h>

#include <QCryptographicHash>
#include <QTemporaryDir>

#include "TestUtils.h"
#include "server/FileEventCollector.h"
#include "server/SnapshotDiff.h"

using fm::ChangeKind;
using fm::DirectorySnapshot;
using fm::FileChange;

TEST(SnapshotDiff, NoChangesForEqualSnapshots)
{
    const DirectorySnapshot snapshot = {{"a", {1, 10}}, {"b", {2, 20}}};
    EXPECT_TRUE(fm::diffSnapshots(snapshot, snapshot).empty());
    EXPECT_TRUE(fm::diffSnapshots({}, {}).empty());
}

TEST(SnapshotDiff, DetectsCreatedModifiedAndDeleted)
{
    const DirectorySnapshot before = {{"kept", {1, 10}}, {"edited", {2, 20}}, {"removed", {3, 30}}};
    const DirectorySnapshot after = {{"kept", {1, 10}}, {"edited", {5, 20}}, {"added", {4, 40}}};

    const std::vector<FileChange> expected = {
        {ChangeKind::Created, "added"},
        {ChangeKind::Modified, "edited"},
        {ChangeKind::Deleted, "removed"},
    };
    EXPECT_EQ(fm::diffSnapshots(before, after), expected);
}

TEST(SnapshotDiff, SameSizeButNewerTimeIsModified)
{
    const DirectorySnapshot before = {{"f", {100, 1000}}};
    const DirectorySnapshot after = {{"f", {100, 2000}}};

    const std::vector<FileChange> expected = {{ChangeKind::Modified, "f"}};
    EXPECT_EQ(fm::diffSnapshots(before, after), expected);
}

TEST(SnapshotDiff, TakeSnapshotListsOnlyFiles)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(fm::test::writeFile(dir.filePath("one.txt"), "12345"));
    ASSERT_TRUE(QDir(dir.path()).mkdir("subdir"));

    const DirectorySnapshot snapshot = fm::takeSnapshot(dir.path());
    ASSERT_EQ(snapshot.size(), 1u);
    EXPECT_EQ(snapshot.at("one.txt").size, 5);
    EXPECT_GT(snapshot.at("one.txt").mtimeMs, 0);
}

TEST(FileEventBuilder, CollectsMetadataAndChecksum)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QByteArray content("hello, file monitor");
    ASSERT_TRUE(fm::test::writeFile(dir.filePath("data.txt"), content));

    const fm::pb::FileEvent event =
        fm::buildFileEvent(dir.path(), {ChangeKind::Created, "data.txt"});

    EXPECT_EQ(event.type(), fm::pb::FILE_CREATED);
    EXPECT_EQ(event.file_name(), "data.txt");
    EXPECT_NE(event.file_path().find("data.txt"), std::string::npos);
    EXPECT_EQ(event.size(), static_cast<quint64>(content.size()));
    EXPECT_GT(event.mtime_ms(), 0);
    EXPECT_EQ(event.sha256(),
              QCryptographicHash::hash(content, QCryptographicHash::Sha256).toStdString());
}

TEST(FileEventBuilder, DeletedFileHasNoSizeOrChecksum)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const fm::pb::FileEvent event = fm::buildFileEvent(dir.path(), {ChangeKind::Deleted, "gone"});

    EXPECT_EQ(event.type(), fm::pb::FILE_DELETED);
    EXPECT_EQ(event.file_name(), "gone");
    EXPECT_EQ(event.size(), 0u);
    EXPECT_TRUE(event.sha256().empty());
    EXPECT_GT(event.mtime_ms(), 0);
}
