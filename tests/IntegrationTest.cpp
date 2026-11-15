// End-to-end tests: a real server and real client connections talking over
// TCP on the loopback interface, inside one event loop.

#include <gtest/gtest.h>

#include <QCryptographicHash>
#include <QTemporaryDir>

#include <algorithm>
#include <memory>
#include <vector>

#include "TestUtils.h"
#include "client/FileDownloader.h"
#include "client/ServerConnection.h"
#include "server/FileMonitorServer.h"

namespace pb = fm::pb;
using fm::ServerConnection;
using fm::test::spin;
using fm::test::waitFor;

namespace {

constexpr int kClientCount = 5;

// A client without a GUI: remembers everything the server sends.
struct TestClient
{
    TestClient()
    {
        QObject::connect(&connection, &ServerConnection::helloReceived,
                         [this](const pb::ServerHello& hello) {
                             ++hellos;
                             monitoring = hello.monitoring_active();
                             watchedDirectory = QString::fromStdString(hello.watched_directory());
                         });
        QObject::connect(&connection, &ServerConnection::fileEventReceived,
                         [this](const pb::FileEvent& event) { events.push_back(event); });
        QObject::connect(&connection, &ServerConnection::monitoringStateReceived,
                         [this](bool active) { monitoring = active; });
        QObject::connect(&connection, &ServerConnection::commandResultReceived,
                         [this](const pb::CommandResult& result) { results.push_back(result); });
        QObject::connect(&connection, &ServerConnection::fileChunkReceived,
                         [this](const pb::FileChunk&) { ++chunks; });
    }

    // The latest event of this type for this file, or nullptr.
    const pb::FileEvent* findEvent(pb::EventType type, const std::string& name) const
    {
        const auto it = std::find_if(events.rbegin(), events.rend(),
                                     [&](const pb::FileEvent& event) {
                                         return event.type() == type && event.file_name() == name;
                                     });
        return it == events.rend() ? nullptr : &*it;
    }

    const pb::CommandResult* findResult(quint64 requestId) const
    {
        const auto it = std::find_if(results.begin(), results.end(),
                                     [requestId](const pb::CommandResult& result) {
                                         return result.request_id() == requestId;
                                     });
        return it == results.end() ? nullptr : &*it;
    }

    ServerConnection connection;
    int hellos = 0;
    int chunks = 0;
    bool monitoring = false;
    QString watchedDirectory;
    std::vector<pb::FileEvent> events;
    std::vector<pb::CommandResult> results;
};

class IntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());
        startServer(0);

        for (int i = 0; i < kClientCount; ++i) {
            m_clients.push_back(std::make_unique<TestClient>());
            m_clients.back()->connection.connectToServer("127.0.0.1", m_port);
        }
        ASSERT_TRUE(waitFor([this] { return allClients([](const TestClient& c) { return c.hellos == 1; }); }));
    }

    void startServer(quint16 port)
    {
        m_server = std::make_unique<fm::FileMonitorServer>(m_dir.path());
        ASSERT_TRUE(m_server->listen(QHostAddress::LocalHost, port))
            << m_server->errorString().toStdString();
        m_port = m_server->port();
    }

    template <typename Predicate>
    bool allClients(Predicate predicate) const
    {
        return std::all_of(m_clients.begin(), m_clients.end(),
                           [&](const std::unique_ptr<TestClient>& client) {
                               return predicate(*client);
                           });
    }

    bool allClientsSaw(pb::EventType type, const std::string& name) const
    {
        return allClients([&](const TestClient& c) { return c.findEvent(type, name) != nullptr; });
    }

    QString path(const QString& name) const { return m_dir.filePath(name); }

    QTemporaryDir m_dir;
    std::unique_ptr<fm::FileMonitorServer> m_server;
    quint16 m_port = 0;
    std::vector<std::unique_ptr<TestClient>> m_clients;
};

QByteArray sha256(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

// Deterministic bytes that include every value, so NULs and invalid UTF-8.
QByteArray patternData(int size)
{
    QByteArray data(size, Qt::Uninitialized);
    quint32 state = 12345;
    for (int i = 0; i < size; ++i) {
        state = state * 1103515245u + 12345u;
        data[i] = static_cast<char>(state >> 16);
    }
    return data;
}

} // namespace

TEST_F(IntegrationTest, FiveClientsConnectAndGetHello)
{
    EXPECT_EQ(m_server->clientCount(), kClientCount);
    for (const auto& client : m_clients) {
        EXPECT_TRUE(client->connection.isConnected());
        EXPECT_TRUE(client->monitoring);
        EXPECT_EQ(QDir(client->watchedDirectory), QDir(m_dir.path()));
    }
}

TEST_F(IntegrationTest, EventsAreBroadcastToAllClients)
{
    const QByteArray content("first version");
    ASSERT_TRUE(fm::test::writeFile(path("doc.txt"), content));
    ASSERT_TRUE(waitFor([this] { return allClientsSaw(pb::FILE_CREATED, "doc.txt"); }));

    for (const auto& client : m_clients) {
        const pb::FileEvent* event = client->findEvent(pb::FILE_CREATED, "doc.txt");
        EXPECT_EQ(event->size(), static_cast<quint64>(content.size()));
        EXPECT_EQ(event->sha256(), sha256(content).toStdString());
        EXPECT_GT(event->mtime_ms(), 0);
        EXPECT_NE(event->file_path().find("doc.txt"), std::string::npos);
    }

    const QByteArray updated("second, longer version");
    ASSERT_TRUE(fm::test::writeFile(path("doc.txt"), updated));
    ASSERT_TRUE(waitFor([this] { return allClientsSaw(pb::FILE_MODIFIED, "doc.txt"); }));
    for (const auto& client : m_clients) {
        const pb::FileEvent* event = client->findEvent(pb::FILE_MODIFIED, "doc.txt");
        EXPECT_EQ(event->size(), static_cast<quint64>(updated.size()));
        EXPECT_EQ(event->sha256(), sha256(updated).toStdString());
    }

    ASSERT_TRUE(QFile::remove(path("doc.txt")));
    ASSERT_TRUE(waitFor([this] { return allClientsSaw(pb::FILE_DELETED, "doc.txt"); }));
}

TEST_F(IntegrationTest, LargeFileIsTransferredInChunks)
{
    // Not a multiple of the chunk size, so the last chunk is shorter.
    const QByteArray content = patternData(3 * 1024 * 1024 + 12345);
    ASSERT_TRUE(fm::test::writeFile(path("big.bin"), content));

    TestClient& client = *m_clients[0];
    fm::FileDownloader downloader;
    QObject::connect(&client.connection, &ServerConnection::fileChunkReceived, &downloader,
                     &fm::FileDownloader::handleChunk);

    QByteArray received;
    bool finished = false;
    QString failure;
    QObject::connect(&downloader, &fm::FileDownloader::finished,
                     [&](const QString&, std::shared_ptr<QTemporaryFile> file) {
                         QFile reader(file->fileName());
                         if (reader.open(QIODevice::ReadOnly))
                             received = reader.readAll();
                         finished = true;
                     });
    QObject::connect(&downloader, &fm::FileDownloader::failed,
                     [&](const QString&, const QString& reason) { failure = reason; });

    const quint64 requestId = client.connection.requestFile("big.bin");
    ASSERT_NE(requestId, 0u);
    downloader.begin(requestId, "big.bin");

    ASSERT_TRUE(waitFor([&] { return finished || !failure.isEmpty(); }));
    ASSERT_TRUE(failure.isEmpty()) << failure.toStdString();
    EXPECT_EQ(received.size(), content.size());
    EXPECT_EQ(sha256(received), sha256(content));

    const int expectedChunks =
        static_cast<int>((content.size() + fm::kFileChunkSize - 1) / fm::kFileChunkSize);
    EXPECT_EQ(client.chunks, expectedChunks);
    // The file went only to the client that asked for it.
    EXPECT_EQ(m_clients[1]->chunks, 0);
}

TEST_F(IntegrationTest, SmallAndEmptyFilesArriveAsOneChunk)
{
    ASSERT_TRUE(fm::test::writeFile(path("small.txt"), "tiny"));
    ASSERT_TRUE(fm::test::writeFile(path("empty.txt"), ""));

    TestClient& client = *m_clients[2];
    std::vector<pb::FileChunk> chunks;
    QObject::connect(&client.connection, &ServerConnection::fileChunkReceived,
                     [&](const pb::FileChunk& chunk) { chunks.push_back(chunk); });

    // Two requests in a row are served in order.
    const quint64 first = client.connection.requestFile("small.txt");
    const quint64 second = client.connection.requestFile("empty.txt");
    ASSERT_TRUE(waitFor([&] { return chunks.size() == 2; }));

    EXPECT_EQ(chunks[0].request_id(), first);
    EXPECT_EQ(chunks[0].file_name(), "small.txt");
    EXPECT_EQ(chunks[0].data(), "tiny");
    EXPECT_EQ(chunks[0].total_size(), 4u);
    EXPECT_EQ(chunks[0].offset(), 0u);
    EXPECT_TRUE(chunks[0].is_last());

    EXPECT_EQ(chunks[1].request_id(), second);
    EXPECT_TRUE(chunks[1].data().empty());
    EXPECT_EQ(chunks[1].total_size(), 0u);
    EXPECT_TRUE(chunks[1].is_last());
}

TEST_F(IntegrationTest, BadFileRequestsAreRefused)
{
    ASSERT_TRUE(fm::test::writeFile(m_dir.path() + "/../fm_outside.txt", "outside"));

    TestClient& client = *m_clients[0];
    const quint64 missing = client.connection.requestFile("missing.txt");
    const quint64 traversal = client.connection.requestFile("../fm_outside.txt");
    const quint64 empty = client.connection.requestFile("");

    ASSERT_TRUE(waitFor([&] { return client.results.size() == 3; }));
    QFile::remove(m_dir.path() + "/../fm_outside.txt");

    for (const quint64 id : {missing, traversal, empty}) {
        const pb::CommandResult* result = client.findResult(id);
        ASSERT_NE(result, nullptr);
        EXPECT_FALSE(result->success());
        EXPECT_FALSE(result->message().empty());
    }
    EXPECT_EQ(client.chunks, 0);
    EXPECT_TRUE(client.connection.isConnected());
}

TEST_F(IntegrationTest, StopAndStartMonitoring)
{
    TestClient& commander = *m_clients[1];

    const quint64 stopId = commander.connection.sendStopMonitoring();
    ASSERT_TRUE(waitFor([this] { return allClients([](const TestClient& c) { return !c.monitoring; }); }));
    ASSERT_TRUE(waitFor([&] { return commander.findResult(stopId) != nullptr; }));
    EXPECT_TRUE(commander.findResult(stopId)->success());
    EXPECT_FALSE(m_server->isMonitoring());
    // Only the client that sent the command gets the result.
    EXPECT_TRUE(m_clients[0]->results.empty());

    ASSERT_TRUE(fm::test::writeFile(path("while_stopped.txt"), "x"));
    spin(600);
    EXPECT_TRUE(allClients([](const TestClient& c) { return c.events.empty(); }));

    const quint64 startId = commander.connection.sendStartMonitoring();
    ASSERT_TRUE(waitFor([this] { return allClients([](const TestClient& c) { return c.monitoring; }); }));
    ASSERT_TRUE(waitFor([&] { return commander.findResult(startId) != nullptr; }));

    ASSERT_TRUE(fm::test::writeFile(path("after_start.txt"), "y"));
    ASSERT_TRUE(waitFor([this] { return allClientsSaw(pb::FILE_CREATED, "after_start.txt"); }));
    // The file created while stopped is not reported afterwards either.
    EXPECT_TRUE(allClients([](const TestClient& c) {
        return c.findEvent(pb::FILE_CREATED, "while_stopped.txt") == nullptr;
    }));
}

TEST_F(IntegrationTest, ServerSurvivesClientDisconnects)
{
    m_clients[0]->connection.disconnectFromServer();
    m_clients.erase(m_clients.begin() + 1); // destroyed while connected
    ASSERT_TRUE(waitFor([this] { return m_server->clientCount() == kClientCount - 2; }));
    EXPECT_EQ(m_clients[0]->connection.state(), ServerConnection::State::Disconnected);

    ASSERT_TRUE(fm::test::writeFile(path("still_works.txt"), "z"));
    ASSERT_TRUE(waitFor([this] {
        return std::all_of(m_clients.begin() + 1, m_clients.end(), [](const auto& client) {
            return client->findEvent(pb::FILE_CREATED, "still_works.txt") != nullptr;
        });
    }));
    EXPECT_TRUE(m_clients[0]->events.empty());
}

TEST_F(IntegrationTest, ClientsReconnectAfterServerRestart)
{
    std::vector<ServerConnection::State> states;
    QObject::connect(&m_clients[0]->connection, &ServerConnection::stateChanged,
                     [&](ServerConnection::State state, const QString&) { states.push_back(state); });

    m_server.reset();
    ASSERT_TRUE(waitFor([this] {
        return allClients([](const TestClient& c) {
            return c.connection.state() == ServerConnection::State::WaitingToReconnect
                || c.connection.state() == ServerConnection::State::Connecting;
        });
    }));
    EXPECT_FALSE(m_clients[0]->connection.isConnected());
    EXPECT_EQ(m_clients[0]->connection.requestFile("x"), 0u);

    startServer(m_port);
    ASSERT_TRUE(waitFor([this] { return allClients([](const TestClient& c) { return c.hellos == 2; }); },
                        30000));
    EXPECT_EQ(m_server->clientCount(), kClientCount);

    ASSERT_FALSE(states.empty());
    EXPECT_EQ(states.front(), ServerConnection::State::WaitingToReconnect);
    EXPECT_EQ(states.back(), ServerConnection::State::Connected);

    // The new connection is fully usable.
    ASSERT_TRUE(fm::test::writeFile(path("after_restart.txt"), "again"));
    ASSERT_TRUE(waitFor([this] { return allClientsSaw(pb::FILE_CREATED, "after_restart.txt"); }));
}

TEST(ServerConnectionTest, UnreachableServerWithoutAutoReconnect)
{
    // Find a port with nothing listening: bind one, then release it.
    quint16 port = 0;
    {
        QTcpServer probe;
        ASSERT_TRUE(probe.listen(QHostAddress::LocalHost, 0));
        port = probe.serverPort();
    }

    ServerConnection connection;
    connection.setAutoReconnect(false);

    std::vector<ServerConnection::State> states;
    QString lastDetail;
    QObject::connect(&connection, &ServerConnection::stateChanged,
                     [&](ServerConnection::State state, const QString& detail) {
                         states.push_back(state);
                         lastDetail = detail;
                     });

    connection.connectToServer("127.0.0.1", port);
    ASSERT_TRUE(waitFor([&] { return states.size() >= 2; }));

    EXPECT_EQ(states[0], ServerConnection::State::Connecting);
    EXPECT_EQ(states[1], ServerConnection::State::Disconnected);
    EXPECT_FALSE(lastDetail.isEmpty());
    EXPECT_EQ(connection.sendStartMonitoring(), 0u);
}

TEST(ServerConnectionTest, RetriesUntilDisconnectIsRequested)
{
    quint16 port = 0;
    {
        QTcpServer probe;
        ASSERT_TRUE(probe.listen(QHostAddress::LocalHost, 0));
        port = probe.serverPort();
    }

    ServerConnection connection;
    int attempts = 0;
    QObject::connect(&connection, &ServerConnection::stateChanged,
                     [&](ServerConnection::State state, const QString&) {
                         if (state == ServerConnection::State::Connecting)
                             ++attempts;
                     });

    connection.connectToServer("127.0.0.1", port);
    ASSERT_TRUE(waitFor([&] { return attempts >= 2; }, 20000));

    connection.disconnectFromServer();
    EXPECT_EQ(connection.state(), ServerConnection::State::Disconnected);
    const int attemptsAtDisconnect = attempts;
    spin(2500);
    EXPECT_EQ(attempts, attemptsAtDisconnect);
}

// A peer that sends garbage must be dropped without harming other clients.
TEST_F(IntegrationTest, ServerDropsClientThatViolatesFraming)
{
    QTcpSocket rogue;
    rogue.connectToHost("127.0.0.1", m_port);
    ASSERT_TRUE(waitFor([&] { return rogue.state() == QAbstractSocket::ConnectedState; }));
    ASSERT_TRUE(waitFor([this] { return m_server->clientCount() == kClientCount + 1; }));

    rogue.write(QByteArray("\xff\xff\xff\xff and then some garbage", 28));
    ASSERT_TRUE(waitFor([&] { return rogue.state() == QAbstractSocket::UnconnectedState; }));
    ASSERT_TRUE(waitFor([this] { return m_server->clientCount() == kClientCount; }));

    ASSERT_TRUE(fm::test::writeFile(path("unharmed.txt"), "ok"));
    ASSERT_TRUE(waitFor([this] { return allClientsSaw(pb::FILE_CREATED, "unharmed.txt"); }));
}
