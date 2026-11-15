#include <gtest/gtest.h>

#include <QTemporaryDir>

#include "TestUtils.h"
#include "client/EventHistoryStore.h"

namespace pb = fm::pb;

TEST(EventHistoryStore, SavesAndLoadsRecords)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    // The folder does not exist yet; save() must create it.
    const fm::EventHistoryStore store(dir.filePath("nested/history.json"));

    std::vector<fm::EventRecord> records(2);
    records[0].time = QDateTime::fromMSecsSinceEpoch(1700000000123);
    records[0].fileName = QString::fromUtf8("отчёт.txt");
    records[0].filePath = "C:\\watched\\report.txt";
    records[0].type = pb::FILE_CREATED;
    records[0].size = 5000000000ull; // over 4 GB
    records[0].sha256 = QByteArray::fromHex("00ff10");
    records[1].time = QDateTime::fromMSecsSinceEpoch(1700000001000);
    records[1].fileName = "gone.log";
    records[1].type = pb::FILE_DELETED;

    ASSERT_TRUE(store.save(records));
    const std::vector<fm::EventRecord> loaded = store.load();

    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_EQ(loaded[0].time, records[0].time);
    EXPECT_EQ(loaded[0].fileName, records[0].fileName);
    EXPECT_EQ(loaded[0].filePath, records[0].filePath);
    EXPECT_EQ(loaded[0].type, pb::FILE_CREATED);
    EXPECT_EQ(loaded[0].size, 5000000000ull);
    EXPECT_EQ(loaded[0].sha256, records[0].sha256);
    EXPECT_EQ(loaded[1].fileName, QString("gone.log"));
    EXPECT_EQ(loaded[1].type, pb::FILE_DELETED);
    EXPECT_TRUE(loaded[1].sha256.isEmpty());
}

TEST(EventHistoryStore, MissingFileGivesEmptyHistory)
{
    QTemporaryDir dir;
    const fm::EventHistoryStore store(dir.filePath("none.json"));
    EXPECT_TRUE(store.load().empty());
}

TEST(EventHistoryStore, DamagedFileGivesEmptyHistory)
{
    QTemporaryDir dir;
    const QString path = dir.filePath("history.json");
    ASSERT_TRUE(fm::test::writeFile(path, "{ this is not json"));
    EXPECT_TRUE(fm::EventHistoryStore(path).load().empty());
}

TEST(EventHistoryStore, UnknownEventTypeBecomesUnspecified)
{
    QTemporaryDir dir;
    const QString path = dir.filePath("history.json");
    ASSERT_TRUE(fm::test::writeFile(
        path, R"({"version":1,"events":[{"file_name":"x","type":99,"time_ms":1}]})"));

    const std::vector<fm::EventRecord> loaded = fm::EventHistoryStore(path).load();
    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded[0].type, pb::EVENT_TYPE_UNSPECIFIED);
}

TEST(EventHistoryStore, SaveReplacesPreviousContent)
{
    QTemporaryDir dir;
    const fm::EventHistoryStore store(dir.filePath("history.json"));

    ASSERT_TRUE(store.save(std::vector<fm::EventRecord>(3)));
    ASSERT_TRUE(store.save(std::vector<fm::EventRecord>(1)));
    EXPECT_EQ(store.load().size(), 1u);
}
