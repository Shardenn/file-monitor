#include <gtest/gtest.h>

#include <QTemporaryDir>

#include "TestUtils.h"
#include "client/FilePreview.h"

namespace {

fm::FilePreview previewOf(const QByteArray& content, qint64 maxBytes = fm::kPreviewLimitBytes)
{
    QTemporaryDir dir;
    const QString path = dir.filePath("file");
    EXPECT_TRUE(fm::test::writeFile(path, content));
    return fm::loadFilePreview(path, maxBytes);
}

} // namespace

TEST(FilePreview, PlainTextIsText)
{
    const fm::FilePreview preview = previewOf("line one\nline two\n");
    EXPECT_TRUE(preview.isText);
    EXPECT_EQ(preview.text, QString("line one\nline two\n"));
    EXPECT_FALSE(preview.truncated);
    EXPECT_EQ(preview.fileSize, 18);
}

TEST(FilePreview, Utf8IsText)
{
    const QString text = QString::fromUtf8("Привет, мир! 日本語");
    const fm::FilePreview preview = previewOf(text.toUtf8());
    EXPECT_TRUE(preview.isText);
    EXPECT_EQ(preview.text, text);
}

TEST(FilePreview, EmptyFileIsText)
{
    const fm::FilePreview preview = previewOf("");
    EXPECT_TRUE(preview.isText);
    EXPECT_TRUE(preview.text.isEmpty());
}

TEST(FilePreview, NulByteMeansBinary)
{
    const fm::FilePreview preview = previewOf(QByteArray("PK\x03\x04\0\0data", 10));
    EXPECT_FALSE(preview.isText);
    EXPECT_TRUE(preview.text.isEmpty());
}

TEST(FilePreview, InvalidUtf8MeansBinary)
{
    const fm::FilePreview preview = previewOf(QByteArray("\xff\xfe\xfa\xfb header"));
    EXPECT_FALSE(preview.isText);
}

TEST(FilePreview, LongTextIsTruncated)
{
    const fm::FilePreview preview = previewOf(QByteArray(1000, 'a'), 100);
    EXPECT_TRUE(preview.isText);
    EXPECT_TRUE(preview.truncated);
    EXPECT_EQ(preview.text.size(), 100);
    EXPECT_EQ(preview.fileSize, 1000);
}

TEST(FilePreview, CutInsideMultiByteCharacterIsStillText)
{
    // Every character takes two bytes; the limit of 5 cuts the third one.
    const fm::FilePreview preview = previewOf(QString::fromUtf8("ЖЖЖЖ").toUtf8(), 5);
    EXPECT_TRUE(preview.isText);
    EXPECT_TRUE(preview.truncated);
}

TEST(FilePreview, MissingFileIsNotText)
{
    const fm::FilePreview preview = fm::loadFilePreview("/no/such/file/anywhere");
    EXPECT_FALSE(preview.isText);
}
