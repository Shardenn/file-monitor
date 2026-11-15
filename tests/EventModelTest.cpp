#include <gtest/gtest.h>

#include "TestUtils.h"
#include "client/EventFilterProxy.h"
#include "client/EventTableModel.h"

namespace pb = fm::pb;

namespace {

fm::EventRecord makeRecord(const QString& name, pb::EventType type, quint64 size = 0)
{
    fm::EventRecord record;
    record.time = QDateTime::fromMSecsSinceEpoch(1700000000000);
    record.fileName = name;
    record.filePath = "/watched/" + name;
    record.type = type;
    record.size = size;
    return record;
}

QStringList visibleNames(const fm::EventFilterProxy& proxy)
{
    QStringList names;
    for (int row = 0; row < proxy.rowCount(); ++row)
        names << proxy.index(row, fm::EventTableModel::NameColumn).data().toString();
    return names;
}

} // namespace

TEST(EventRecord, ConvertsFromProto)
{
    pb::FileEvent event;
    event.set_type(pb::FILE_MODIFIED);
    event.set_file_name("a.txt");
    event.set_file_path("/dir/a.txt");
    event.set_size(2048);
    event.set_mtime_ms(1700000000123);
    event.set_sha256(std::string("\x01\x02\x03", 3));

    const fm::EventRecord record = fm::EventRecord::fromProto(event);
    EXPECT_EQ(record.fileName, QString("a.txt"));
    EXPECT_EQ(record.filePath, QString("/dir/a.txt"));
    EXPECT_EQ(record.type, pb::FILE_MODIFIED);
    EXPECT_EQ(record.size, 2048u);
    EXPECT_EQ(record.time.toMSecsSinceEpoch(), 1700000000123);
    EXPECT_EQ(record.sha256, QByteArray("\x01\x02\x03", 3));
}

TEST(EventRecord, FormatsSizes)
{
    EXPECT_EQ(fm::formatFileSize(0), QString("0 B"));
    EXPECT_EQ(fm::formatFileSize(1023), QString("1023 B"));
    EXPECT_EQ(fm::formatFileSize(1024), QString("1.0 KB"));
    EXPECT_EQ(fm::formatFileSize(1536), QString("1.5 KB"));
    EXPECT_EQ(fm::formatFileSize(5 * 1024 * 1024), QString("5.0 MB"));
    EXPECT_EQ(fm::formatFileSize(3ull * 1024 * 1024 * 1024), QString("3.0 GB"));
}

TEST(EventTableModel, ShowsTheFourColumns)
{
    fm::EventTableModel model;
    model.append(makeRecord("report.txt", pb::FILE_CREATED, 1536));
    model.append(makeRecord("old.log", pb::FILE_DELETED));

    ASSERT_EQ(model.rowCount(), 2);
    ASSERT_EQ(model.columnCount(), 4);
    EXPECT_EQ(model.headerData(fm::EventTableModel::NameColumn, Qt::Horizontal).toString(),
              QString("File name"));
    EXPECT_FALSE(model.index(0, fm::EventTableModel::TimeColumn).data().toString().isEmpty());
    EXPECT_EQ(model.index(0, fm::EventTableModel::NameColumn).data().toString(),
              QString("report.txt"));
    EXPECT_EQ(model.index(0, fm::EventTableModel::TypeColumn).data().toString(),
              QString("Created"));
    EXPECT_EQ(model.index(0, fm::EventTableModel::SizeColumn).data().toString(),
              QString("1.5 KB"));
    EXPECT_EQ(model.index(1, fm::EventTableModel::TypeColumn).data().toString(),
              QString("Deleted"));
    EXPECT_EQ(model.index(1, fm::EventTableModel::SizeColumn).data().toString(), QString("-"));
}

TEST(EventTableModel, DropsOldestRowsBeyondTheLimit)
{
    fm::EventTableModel model;
    const int total = fm::EventTableModel::kMaxRows + 5;
    for (int i = 0; i < total; ++i)
        model.append(makeRecord(QString::number(i), pb::FILE_CREATED));

    ASSERT_EQ(model.rowCount(), fm::EventTableModel::kMaxRows);
    EXPECT_EQ(model.recordAt(0).fileName, QString("5"));
    EXPECT_EQ(model.recordAt(model.rowCount() - 1).fileName, QString::number(total - 1));

    model.clear();
    EXPECT_EQ(model.rowCount(), 0);
}

TEST(EventFilterProxy, WildcardMatching)
{
    EXPECT_TRUE(fm::matchesWildcard("", ""));
    EXPECT_FALSE(fm::matchesWildcard("", "a"));
    EXPECT_TRUE(fm::matchesWildcard("*", ""));
    EXPECT_TRUE(fm::matchesWildcard("*", "anything.txt"));
    EXPECT_TRUE(fm::matchesWildcard("*.txt", "notes.txt"));
    EXPECT_FALSE(fm::matchesWildcard("*.txt", "notes.txt.bak"));
    EXPECT_TRUE(fm::matchesWildcard("a*b*c", "a-b-b-c"));
    EXPECT_FALSE(fm::matchesWildcard("a*b*c", "a-b-b"));
    EXPECT_TRUE(fm::matchesWildcard("?.txt", "a.txt"));
    EXPECT_FALSE(fm::matchesWildcard("?.txt", ".txt"));
    EXPECT_FALSE(fm::matchesWildcard("?", ""));
    EXPECT_TRUE(fm::matchesWildcard("REPORT*", "report.PDF"));
    EXPECT_TRUE(fm::matchesWildcard("***a***", "a"));
    EXPECT_TRUE(fm::matchesWildcard("*aab", "aaab"));
    EXPECT_FALSE(fm::matchesWildcard("a.b", "axb")); // the dot is literal
    EXPECT_TRUE(fm::matchesWildcard(QString::fromUtf8("*ЁЖ*"), QString::fromUtf8("чижик-ёжик")));
}

TEST(EventFilterProxy, FiltersByTypeAndName)
{
    fm::EventTableModel model;
    model.append(makeRecord("report.txt", pb::FILE_CREATED));
    model.append(makeRecord("Report.pdf", pb::FILE_MODIFIED));
    model.append(makeRecord("notes.txt", pb::FILE_DELETED));
    model.append(makeRecord("image.png", pb::FILE_CREATED));

    fm::EventFilterProxy proxy;
    proxy.setSourceModel(&model);
    EXPECT_EQ(proxy.rowCount(), 4);

    proxy.setTypeFilter(pb::FILE_CREATED);
    EXPECT_EQ(visibleNames(proxy), QStringList({"report.txt", "image.png"}));

    proxy.setNameFilter("REPORT"); // substring, case-insensitive
    EXPECT_EQ(visibleNames(proxy), QStringList({"report.txt"}));

    proxy.setTypeFilter(pb::EVENT_TYPE_UNSPECIFIED);
    EXPECT_EQ(visibleNames(proxy), QStringList({"report.txt", "Report.pdf"}));

    proxy.setNameFilter("*.txt");
    EXPECT_EQ(visibleNames(proxy), QStringList({"report.txt", "notes.txt"}));

    proxy.setNameFilter("?otes.txt");
    EXPECT_EQ(visibleNames(proxy), QStringList({"notes.txt"}));

    proxy.setNameFilter("a.b"); // the dot is literal, not "any character"
    EXPECT_EQ(proxy.rowCount(), 0);

    proxy.setNameFilter("  ");
    EXPECT_EQ(proxy.rowCount(), 4);

    // New rows pass through the active filter.
    proxy.setTypeFilter(pb::FILE_DELETED);
    model.append(makeRecord("late.tmp", pb::FILE_DELETED));
    EXPECT_EQ(visibleNames(proxy), QStringList({"notes.txt", "late.tmp"}));
}
