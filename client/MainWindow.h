#pragma once

#include <QFutureWatcher>
#include <QMainWindow>

#include <memory>

#include "client/EventFilterProxy.h"
#include "client/EventHistoryStore.h"
#include "client/EventTableModel.h"
#include "client/FileDownloader.h"
#include "client/FilePreview.h"
#include "client/ServerConnection.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableView;

namespace fm {

// The client window. It only lays out the widgets and connects them to the
// components that do the work (connection, models, downloader, history).
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // The event history is loaded from and saved to `historyFilePath`.
    explicit MainWindow(const QString& historyFilePath, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    QWidget* createConnectionBar();
    QWidget* createFilterBar();
    QWidget* createActionBar();
    void connectSignals();
    void setObjectNames();

    void toggleConnection();
    void onConnectionStateChanged(ServerConnection::State state, const QString& detail);
    void onHello(const pb::ServerHello& hello);
    void onFileEvent(const pb::FileEvent& event);
    void onCommandResult(const pb::CommandResult& result);
    void onMonitoringState(bool active);

    void downloadSelectedFile();
    void onDownloadFinished(const QString& fileName, std::shared_ptr<QTemporaryFile> file);
    void onDownloadFailed(const QString& fileName, const QString& reason);
    void onPreviewLoaded();

    void updateControls();

    ServerConnection m_connection;
    EventTableModel m_model;
    EventFilterProxy m_filter;
    FileDownloader m_downloader;
    EventHistoryStore m_history;

    // The preview is read on a worker thread; the temporary file is kept
    // alive until the dialog has its content.
    QFutureWatcher<FilePreview> m_previewWatcher;
    std::shared_ptr<QTemporaryFile> m_previewFile;
    QString m_previewFileName;

    bool m_monitoringKnown = false;
    bool m_monitoringActive = false;

    QLineEdit* m_hostEdit = nullptr;
    QSpinBox* m_portSpin = nullptr;
    QPushButton* m_connectButton = nullptr;
    QCheckBox* m_autoReconnectCheck = nullptr;
    QLabel* m_connectionLabel = nullptr;
    QLabel* m_monitoringLabel = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QComboBox* m_typeFilter = nullptr;
    QLineEdit* m_nameFilter = nullptr;
    QTableView* m_table = nullptr;
    QPushButton* m_downloadButton = nullptr;
    QProgressBar* m_progress = nullptr;
};

} // namespace fm
