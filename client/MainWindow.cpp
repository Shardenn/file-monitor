#include "client/MainWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableView>
#include <QVBoxLayout>
#include <QtConcurrent>

#include "client/FilePreviewDialog.h"

namespace fm {

namespace {
constexpr int kStatusMessageMs = 8000;
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_history(EventHistoryStore::defaultFilePath())
{
    setWindowTitle(QStringLiteral("File Monitor"));
    resize(860, 600);

    m_filter.setSourceModel(&m_model);

    m_table = new QTableView(this);
    m_table->setModel(&m_filter);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(EventTableModel::NameColumn,
                                                      QHeaderView::Stretch);
    m_table->setColumnWidth(EventTableModel::TimeColumn, 160);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->addWidget(createConnectionBar());
    layout->addWidget(createFilterBar());
    layout->addWidget(m_table, 1);
    layout->addWidget(createActionBar());
    setCentralWidget(central);

    connectSignals();

    m_model.setRecords(m_history.load());
    m_table->scrollToBottom();
    onConnectionStateChanged(ServerConnection::State::Disconnected, QString());
}

MainWindow::~MainWindow()
{
    m_history.save(m_model.records());
    // The worker only reads a small part of a local file, so this is short.
    m_previewWatcher.waitForFinished();
}

QWidget* MainWindow::createConnectionBar()
{
    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), this);
    m_hostEdit->setPlaceholderText(QStringLiteral("Server IP or host name"));

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(kDefaultPort);

    m_connectButton = new QPushButton(this);
    m_autoReconnectCheck = new QCheckBox(QStringLiteral("Auto-reconnect"), this);
    m_autoReconnectCheck->setChecked(m_connection.autoReconnect());

    m_connectionLabel = new QLabel(this);
    m_monitoringLabel = new QLabel(this);
    m_startButton = new QPushButton(QStringLiteral("Start monitoring"), this);
    m_stopButton = new QPushButton(QStringLiteral("Stop monitoring"), this);

    auto* bar = new QWidget(this);
    auto* rows = new QVBoxLayout(bar);
    rows->setContentsMargins(0, 0, 0, 0);

    auto* connectionRow = new QHBoxLayout;
    connectionRow->addWidget(new QLabel(QStringLiteral("Server:"), this));
    connectionRow->addWidget(m_hostEdit, 1);
    connectionRow->addWidget(new QLabel(QStringLiteral("Port:"), this));
    connectionRow->addWidget(m_portSpin);
    connectionRow->addWidget(m_connectButton);
    connectionRow->addWidget(m_autoReconnectCheck);
    rows->addLayout(connectionRow);

    auto* stateRow = new QHBoxLayout;
    stateRow->addWidget(m_connectionLabel, 1);
    stateRow->addWidget(m_monitoringLabel);
    stateRow->addWidget(m_startButton);
    stateRow->addWidget(m_stopButton);
    rows->addLayout(stateRow);

    return bar;
}

QWidget* MainWindow::createFilterBar()
{
    m_typeFilter = new QComboBox(this);
    m_typeFilter->addItem(QStringLiteral("All events"), static_cast<int>(pb::EVENT_TYPE_UNSPECIFIED));
    for (const pb::EventType type : {pb::FILE_CREATED, pb::FILE_MODIFIED, pb::FILE_DELETED})
        m_typeFilter->addItem(eventTypeName(type), static_cast<int>(type));

    m_nameFilter = new QLineEdit(this);
    m_nameFilter->setPlaceholderText(QStringLiteral("Filter by file name (* and ? allowed)"));
    m_nameFilter->setClearButtonEnabled(true);

    auto* clearButton = new QPushButton(QStringLiteral("Clear history"), this);
    connect(clearButton, &QPushButton::clicked, &m_model, &EventTableModel::clear);

    auto* bar = new QWidget(this);
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(new QLabel(QStringLiteral("Show:"), this));
    row->addWidget(m_typeFilter);
    row->addWidget(m_nameFilter, 1);
    row->addWidget(clearButton);
    return bar;
}

QWidget* MainWindow::createActionBar()
{
    m_downloadButton = new QPushButton(QStringLiteral("Download and preview"), this);
    m_progress = new QProgressBar(this);
    m_progress->setVisible(false);

    auto* bar = new QWidget(this);
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(m_downloadButton);
    row->addWidget(m_progress, 1);
    row->addStretch();
    return bar;
}

void MainWindow::connectSignals()
{
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    connect(m_hostEdit, &QLineEdit::returnPressed, this, &MainWindow::toggleConnection);
    connect(m_autoReconnectCheck, &QCheckBox::toggled, &m_connection,
            &ServerConnection::setAutoReconnect);
    connect(m_startButton, &QPushButton::clicked, &m_connection,
            &ServerConnection::sendStartMonitoring);
    connect(m_stopButton, &QPushButton::clicked, &m_connection,
            &ServerConnection::sendStopMonitoring);

    connect(&m_connection, &ServerConnection::stateChanged, this,
            &MainWindow::onConnectionStateChanged);
    connect(&m_connection, &ServerConnection::helloReceived, this, &MainWindow::onHello);
    connect(&m_connection, &ServerConnection::fileEventReceived, this, &MainWindow::onFileEvent);
    connect(&m_connection, &ServerConnection::commandResultReceived, this,
            &MainWindow::onCommandResult);
    connect(&m_connection, &ServerConnection::monitoringStateReceived, this,
            &MainWindow::onMonitoringState);
    connect(&m_connection, &ServerConnection::fileChunkReceived, &m_downloader,
            &FileDownloader::handleChunk);

    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        m_filter.setTypeFilter(static_cast<pb::EventType>(m_typeFilter->currentData().toInt()));
    });
    connect(m_nameFilter, &QLineEdit::textChanged, &m_filter, &EventFilterProxy::setNameFilter);

    connect(m_downloadButton, &QPushButton::clicked, this, &MainWindow::downloadSelectedFile);
    connect(m_table, &QTableView::doubleClicked, this, &MainWindow::downloadSelectedFile);
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::updateControls);

    connect(&m_downloader, &FileDownloader::progress, this, [this](qint64 received, qint64 total) {
        // The bar works in KB so that files over 2 GB do not overflow int.
        m_progress->setRange(0, static_cast<int>(total / 1024));
        m_progress->setValue(static_cast<int>(received / 1024));
    });
    connect(&m_downloader, &FileDownloader::finished, this, &MainWindow::onDownloadFinished);
    connect(&m_downloader, &FileDownloader::failed, this, &MainWindow::onDownloadFailed);
    connect(&m_previewWatcher, &QFutureWatcherBase::finished, this,
            &MainWindow::onPreviewLoaded);
}

void MainWindow::toggleConnection()
{
    if (m_connection.state() == ServerConnection::State::Disconnected) {
        const QString host = m_hostEdit->text().trimmed();
        if (host.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("Enter the server address"), kStatusMessageMs);
            return;
        }
        m_connection.connectToServer(host, static_cast<quint16>(m_portSpin->value()));
    } else {
        m_connection.disconnectFromServer();
    }
}

void MainWindow::onConnectionStateChanged(ServerConnection::State state, const QString& detail)
{
    QString text;
    QString color;
    switch (state) {
    case ServerConnection::State::Disconnected:
        text = detail.isEmpty() ? QStringLiteral("Disconnected")
                                : QStringLiteral("Disconnected. %1").arg(detail);
        color = QStringLiteral("#b00020");
        break;
    case ServerConnection::State::Connecting:
        text = QStringLiteral("Connecting to %1...").arg(detail);
        color = QStringLiteral("#9a6700");
        break;
    case ServerConnection::State::Connected:
        text = QStringLiteral("Connected to %1").arg(detail);
        color = QStringLiteral("#1a7f37");
        break;
    case ServerConnection::State::WaitingToReconnect:
        text = detail;
        color = QStringLiteral("#9a6700");
        break;
    }
    m_connectionLabel->setText(text);
    m_connectionLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(color));

    if (state != ServerConnection::State::Connected) {
        m_monitoringKnown = false;
        m_downloader.abort(QStringLiteral("The connection was lost"));
    }
    updateControls();
}

void MainWindow::onHello(const pb::ServerHello& hello)
{
    if (hello.protocol_version() != kProtocolVersion) {
        statusBar()->showMessage(
            QStringLiteral("Warning: the server speaks protocol version %1, this client %2")
                .arg(hello.protocol_version())
                .arg(kProtocolVersion));
    } else {
        statusBar()->showMessage(
            QStringLiteral("Server is watching %1")
                .arg(QString::fromStdString(hello.watched_directory())),
            kStatusMessageMs);
    }
    onMonitoringState(hello.monitoring_active());
}

void MainWindow::onFileEvent(const pb::FileEvent& event)
{
    m_model.append(EventRecord::fromProto(event));
    m_table->scrollToBottom();
}

void MainWindow::onCommandResult(const pb::CommandResult& result)
{
    m_downloader.handleCommandResult(result);

    const QString message = QString::fromStdString(result.message());
    statusBar()->showMessage(result.success() ? message : QStringLiteral("Error: %1").arg(message),
                             kStatusMessageMs);
}

void MainWindow::onMonitoringState(bool active)
{
    m_monitoringKnown = true;
    m_monitoringActive = active;
    updateControls();
}

void MainWindow::downloadSelectedFile()
{
    const QModelIndexList selection = m_table->selectionModel()->selectedRows();
    if (selection.isEmpty() || !m_connection.isConnected() || m_downloader.isActive())
        return;

    const int row = m_filter.mapToSource(selection.first()).row();
    const EventRecord& record = m_model.recordAt(row);

    const quint64 requestId = m_connection.requestFile(record.fileName);
    if (requestId == 0)
        return;

    m_downloader.begin(requestId, record.fileName);
    m_progress->setRange(0, 0); // busy until the first chunk tells the size
    statusBar()->showMessage(QStringLiteral("Downloading %1...").arg(record.fileName));
    updateControls();
}

void MainWindow::onDownloadFinished(const QString& fileName, std::shared_ptr<QTemporaryFile> file)
{
    statusBar()->showMessage(QStringLiteral("Downloaded %1").arg(fileName), kStatusMessageMs);
    updateControls();

    // A previous preview may still be loading; let it finish first so the
    // watcher is never re-targeted while running.
    m_previewWatcher.waitForFinished();

    m_previewFile = std::move(file);
    m_previewFileName = fileName;
    const QString path = m_previewFile->fileName();
    m_previewWatcher.setFuture(QtConcurrent::run([path] { return loadFilePreview(path); }));
}

void MainWindow::onDownloadFailed(const QString& fileName, const QString& reason)
{
    statusBar()->showMessage(QStringLiteral("Download of %1 failed: %2").arg(fileName, reason),
                             kStatusMessageMs);
    updateControls();
}

void MainWindow::onPreviewLoaded()
{
    const FilePreview preview = m_previewWatcher.result();
    m_previewFile.reset(); // the content is in memory now; delete the temporary file

    auto* dialog = new FilePreviewDialog(m_previewFileName, preview, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::updateControls()
{
    const ServerConnection::State state = m_connection.state();
    const bool connected = state == ServerConnection::State::Connected;
    const bool idle = state == ServerConnection::State::Disconnected;

    m_connectButton->setText(idle ? QStringLiteral("Connect")
                                  : connected ? QStringLiteral("Disconnect")
                                              : QStringLiteral("Cancel"));
    m_hostEdit->setEnabled(idle);
    m_portSpin->setEnabled(idle);

    const bool known = connected && m_monitoringKnown;
    m_monitoringLabel->setText(!known ? QStringLiteral("Monitoring: unknown")
                                      : m_monitoringActive ? QStringLiteral("Monitoring: running")
                                                           : QStringLiteral("Monitoring: stopped"));
    m_startButton->setEnabled(known && !m_monitoringActive);
    m_stopButton->setEnabled(known && m_monitoringActive);

    const bool downloading = m_downloader.isActive();
    const bool hasSelection = m_table->selectionModel()->hasSelection();
    m_downloadButton->setEnabled(connected && hasSelection && !downloading);
    m_progress->setVisible(downloading);
}

} // namespace fm
