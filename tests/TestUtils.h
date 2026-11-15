#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QString>
#include <QTimer>

#include <ostream>

// Lets gtest print Qt strings in failure messages.
inline void PrintTo(const QString& value, std::ostream* os)
{
    *os << value.toStdString();
}

namespace fm {
namespace test {

// Runs the event loop until the condition holds or the time is up.
template <typename Condition>
bool waitFor(Condition condition, int timeoutMs = 15000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > timeoutMs)
            return false;
        QEventLoop loop;
        QTimer::singleShot(10, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return true;
}

// Runs the event loop for a fixed time.
inline void spin(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

inline bool writeFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

} // namespace test
} // namespace fm
