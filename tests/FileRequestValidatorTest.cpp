#include <gtest/gtest.h>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include "TestUtils.h"
#include "server/FileRequestValidator.h"

namespace {

class FileRequestValidatorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_root.isValid());
        ASSERT_TRUE(QDir(m_root.path()).mkdir("watched"));
        m_watched = m_root.filePath("watched");
        ASSERT_TRUE(fm::test::writeFile(m_watched + "/inside.txt", "ok"));
        ASSERT_TRUE(fm::test::writeFile(m_root.filePath("secret.txt"), "secret"));
        ASSERT_TRUE(QDir(m_watched).mkdir("subdir"));
    }

    QString resolve(const QString& name)
    {
        m_error.clear();
        return fm::resolveRequestedFile(m_watched, name, &m_error);
    }

    QTemporaryDir m_root;
    QString m_watched;
    QString m_error;
};

} // namespace

TEST_F(FileRequestValidatorTest, ResolvesFileInsideDirectory)
{
    const QString path = resolve("inside.txt");
    ASSERT_FALSE(path.isEmpty()) << m_error.toStdString();
    EXPECT_EQ(QFileInfo(path).fileName(), QString("inside.txt"));
    EXPECT_TRUE(QFileInfo(path).isAbsolute());
    EXPECT_TRUE(m_error.isEmpty());
}

TEST_F(FileRequestValidatorTest, RejectsPathTraversal)
{
    EXPECT_TRUE(resolve("../secret.txt").isEmpty());
    EXPECT_FALSE(m_error.isEmpty());
    EXPECT_TRUE(resolve("..\\secret.txt").isEmpty());
    EXPECT_TRUE(resolve("..").isEmpty());
    EXPECT_TRUE(resolve("subdir/../../secret.txt").isEmpty());
}

TEST_F(FileRequestValidatorTest, RejectsAbsolutePaths)
{
    EXPECT_TRUE(resolve(m_root.filePath("secret.txt")).isEmpty());
    EXPECT_TRUE(resolve("C:\\Windows\\win.ini").isEmpty());
    EXPECT_TRUE(resolve("/etc/passwd").isEmpty());
}

TEST_F(FileRequestValidatorTest, RejectsMissingFilesDirectoriesAndEmptyNames)
{
    EXPECT_TRUE(resolve("missing.txt").isEmpty());
    EXPECT_TRUE(resolve("subdir").isEmpty());
    EXPECT_TRUE(resolve("").isEmpty());
    EXPECT_TRUE(resolve(".").isEmpty());
}

#ifndef Q_OS_WIN
TEST_F(FileRequestValidatorTest, RejectsSymlinkLeavingTheDirectory)
{
    ASSERT_TRUE(QFile::link(m_root.filePath("secret.txt"), m_watched + "/link.txt"));
    EXPECT_TRUE(resolve("link.txt").isEmpty());
}
#endif
