// Drives the real client window against a real server: the same steps a
// user would do with the mouse.
//
// Set FM_SCREENSHOT_DIR to a folder to get PNG screenshots of the window.

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QTemporaryDir>

#include <memory>

#include "TestUtils.h"
#include "client/FilePreviewDialog.h"
#include "client/MainWindow.h"
#include "server/FileMonitorServer.h"

using fm::test::waitFor;

namespace {

class MainWindowTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());
        ASSERT_TRUE(m_settings.isValid());
        startServer(0);

        m_window = std::make_unique<fm::MainWindow>(m_settings.filePath("history.json"));
        m_window->show();
    }

    void startServer(quint16 port)
    {
        m_server = std::make_unique<fm::FileMonitorServer>(m_dir.path());
        ASSERT_TRUE(m_server->listen(QHostAddress::LocalHost, port));
        m_port = m_server->port();
    }

    template <typename Widget>
    Widget* widget(const char* name) const
    {
        Widget* found = m_window->findChild<Widget*>(QString::fromLatin1(name));
        EXPECT_NE(found, nullptr) << "no widget named " << name;
        return found;
    }

    QString labelText(const char* name) const { return widget<QLabel>(name)->text(); }

    void connectToServer()
    {
        widget<QLineEdit>("hostEdit")->setText("127.0.0.1");
        widget<QSpinBox>("portSpin")->setValue(m_port);
        widget<QPushButton>("connectButton")->click();
        ASSERT_TRUE(waitFor([this] { return labelText("connectionLabel").startsWith("Connected"); }));
        ASSERT_TRUE(waitFor([this] { return labelText("monitoringLabel") == "Monitoring: running"; }));
    }

    int rowCount() const { return widget<QTableView>("eventTable")->model()->rowCount(); }

    // Selects the newest row showing this file name and clicks Download.
    void downloadFile(const QString& fileName)
    {
        QTableView* table = widget<QTableView>("eventTable");
        for (int row = table->model()->rowCount() - 1; row >= 0; --row) {
            if (table->model()->index(row, 1).data().toString() == fileName) {
                table->selectRow(row);
                widget<QPushButton>("downloadButton")->click();
                return;
            }
        }
        FAIL() << "no row for " << fileName.toStdString();
    }

    fm::FilePreviewDialog* waitForPreview()
    {
        fm::FilePreviewDialog* dialog = nullptr;
        waitFor([&] {
            dialog = m_window->findChild<fm::FilePreviewDialog*>();
            return dialog != nullptr;
        });
        return dialog;
    }

    void closePreview(fm::FilePreviewDialog* dialog)
    {
        dialog->close(); // deletes itself
        ASSERT_TRUE(waitFor(
            [this] { return m_window->findChild<fm::FilePreviewDialog*>() == nullptr; }));
    }

    void screenshot(const char* name)
    {
        const QString dir = qEnvironmentVariable("FM_SCREENSHOT_DIR");
        if (dir.isEmpty())
            return;
        fm::test::spin(100); // let pending repaints happen
        m_window->grab().save(QDir(dir).filePath(QString::fromLatin1(name) + ".png"));
        if (auto* dialog = m_window->findChild<fm::FilePreviewDialog*>())
            dialog->grab().save(QDir(dir).filePath(QString::fromLatin1(name) + "_dialog.png"));
    }

    QTemporaryDir m_dir;
    QTemporaryDir m_settings;
    std::unique_ptr<fm::FileMonitorServer> m_server;
    quint16 m_port = 0;
    std::unique_ptr<fm::MainWindow> m_window;
};

} // namespace

TEST_F(MainWindowTest, StartsDisconnected)
{
    EXPECT_EQ(labelText("connectionLabel"), QString("Disconnected"));
    EXPECT_EQ(widget<QPushButton>("connectButton")->text(), QString("Connect"));
    EXPECT_FALSE(widget<QPushButton>("startButton")->isEnabled());
    EXPECT_FALSE(widget<QPushButton>("stopButton")->isEnabled());
    EXPECT_FALSE(widget<QPushButton>("downloadButton")->isEnabled());
    EXPECT_EQ(rowCount(), 0);
    screenshot("1_disconnected");
}

TEST_F(MainWindowTest, ShowsEventsAndFiltersThem)
{
    connectToServer();

    ASSERT_TRUE(fm::test::writeFile(m_dir.filePath("notes.txt"), "some notes"));
    ASSERT_TRUE(fm::test::writeFile(m_dir.filePath("photo.bin"), QByteArray(2048, '\0')));
    ASSERT_TRUE(waitFor([this] { return rowCount() >= 2; }));
    ASSERT_TRUE(QFile::remove(m_dir.filePath("notes.txt")));
    ASSERT_TRUE(waitFor([this] { return rowCount() >= 3; }));
    screenshot("2_events");

    const int allRows = rowCount();
    QComboBox* typeFilter = widget<QComboBox>("typeFilter");
    typeFilter->setCurrentIndex(typeFilter->findText("Deleted"));
    EXPECT_EQ(rowCount(), 1);

    typeFilter->setCurrentIndex(0);
    widget<QLineEdit>("nameFilter")->setText("*.bin");
    EXPECT_GE(rowCount(), 1);
    EXPECT_LT(rowCount(), allRows);
    screenshot("3_filtered");

    widget<QLineEdit>("nameFilter")->clear();
    EXPECT_EQ(rowCount(), allRows);
}

TEST_F(MainWindowTest, PreviewsTextAndRefusesBinary)
{
    connectToServer();

    const QByteArray text("Hello from the server!\nSecond line.\n");
    ASSERT_TRUE(fm::test::writeFile(m_dir.filePath("readme.txt"), text));
    ASSERT_TRUE(fm::test::writeFile(m_dir.filePath("data.bin"), QByteArray("\x00\x01\x02\xff", 4)));
    ASSERT_TRUE(waitFor([this] { return rowCount() >= 2; }));

    downloadFile("readme.txt");
    fm::FilePreviewDialog* dialog = waitForPreview();
    ASSERT_NE(dialog, nullptr);
    auto* viewer = dialog->findChild<QPlainTextEdit*>("previewText");
    ASSERT_NE(viewer, nullptr);
    EXPECT_EQ(viewer->toPlainText(), QString::fromUtf8(text));
    EXPECT_TRUE(viewer->isReadOnly());
    screenshot("4_text_preview");
    closePreview(dialog);

    downloadFile("data.bin");
    dialog = waitForPreview();
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->findChild<QPlainTextEdit*>(), nullptr);
    auto* message = dialog->findChild<QLabel*>("noPreviewLabel");
    ASSERT_NE(message, nullptr);
    EXPECT_TRUE(message->text().contains("No preview available"));
    screenshot("5_binary_preview");
    closePreview(dialog);
}

TEST_F(MainWindowTest, DownloadsLargeFileAndShowsTruncatedPreview)
{
    connectToServer();

    const QByteArray line("0123456789 the quick brown fox jumps over the lazy dog\n");
    QByteArray content;
    while (content.size() < 5 * 1024 * 1024)
        content += line;
    ASSERT_TRUE(fm::test::writeFile(m_dir.filePath("big.log"), content));
    ASSERT_TRUE(waitFor([this] { return rowCount() >= 1; }));

    downloadFile("big.log");
    fm::FilePreviewDialog* dialog = waitForPreview();
    ASSERT_NE(dialog, nullptr);
    auto* viewer = dialog->findChild<QPlainTextEdit*>("previewText");
    ASSERT_NE(viewer, nullptr);
    EXPECT_EQ(viewer->toPlainText().size(), static_cast<int>(fm::kPreviewLimitBytes));
    EXPECT_TRUE(viewer->toPlainText().startsWith(QString::fromLatin1(line)));
    closePreview(dialog);

    EXPECT_TRUE(widget<QPushButton>("downloadButton")->isEnabled());
}

TEST_F(MainWindowTest, StopAndStartMonitoringButtons)
{
    connectToServer();
    QPushButton* start = widget<QPushButton>("startButton");
    QPushButton* stop = widget<QPushButton>("stopButton");
    EXPECT_FALSE(start->isEnabled());
    EXPECT_TRUE(stop->isEnabled());

    stop->click();
    ASSERT_TRUE(waitFor([this] { return labelText("monitoringLabel") == "Monitoring: stopped"; }));
    EXPECT_TRUE(start->isEnabled());
    EXPECT_FALSE(stop->isEnabled());
    EXPECT_FALSE(m_server->isMonitoring());

    start->click();
    ASSERT_TRUE(waitFor([this] { return labelText("monitoringLabel") == "Monitoring: running"; }));
    EXPECT_TRUE(m_server->isMonitoring());
}

TEST_F(MainWindowTest, ShowsLostConnectionAndReconnects)
{
    connectToServer();

    m_server.reset();
    ASSERT_TRUE(waitFor([this] { return labelText("connectionLabel").contains("Retrying"); }));
    EXPECT_EQ(widget<QPushButton>("connectButton")->text(), QString("Cancel"));
    EXPECT_EQ(labelText("monitoringLabel"), QString("Monitoring: unknown"));
    EXPECT_FALSE(widget<QPushButton>("stopButton")->isEnabled());
    screenshot("6_reconnecting");

    startServer(m_port);
    ASSERT_TRUE(waitFor([this] { return labelText("connectionLabel").startsWith("Connected"); },
                        30000));
    ASSERT_TRUE(waitFor([this] { return labelText("monitoringLabel") == "Monitoring: running"; }));
}

TEST_F(MainWindowTest, ManualDisconnectAndUnreachableServer)
{
    connectToServer();
    widget<QPushButton>("connectButton")->click(); // Disconnect
    EXPECT_EQ(labelText("connectionLabel"), QString("Disconnected"));
    EXPECT_TRUE(widget<QLineEdit>("hostEdit")->isEnabled());

    m_server.reset();
    widget<QCheckBox>("autoReconnectCheck")->setChecked(false);
    widget<QPushButton>("connectButton")->click();
    ASSERT_TRUE(waitFor([this] { return labelText("connectionLabel").startsWith("Disconnected."); }));
    EXPECT_TRUE(labelText("connectionLabel").contains("Cannot connect"));
    EXPECT_EQ(widget<QPushButton>("connectButton")->text(), QString("Connect"));
}

TEST_F(MainWindowTest, HistorySurvivesRestart)
{
    connectToServer();
    ASSERT_TRUE(fm::test::writeFile(m_dir.filePath("kept.txt"), "x"));
    ASSERT_TRUE(waitFor([this] { return rowCount() >= 1; }));
    const int rows = rowCount();

    m_window.reset(); // saves the history
    m_window = std::make_unique<fm::MainWindow>(m_settings.filePath("history.json"));
    m_window->show();

    EXPECT_EQ(rowCount(), rows);
    EXPECT_EQ(widget<QTableView>("eventTable")->model()->index(0, 1).data().toString(),
              QString("kept.txt"));
}
