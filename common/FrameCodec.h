#pragma once

#include <QByteArray>
#include <QString>

#include "common/Protocol.h"

namespace fm {

// Serializes an Envelope and prepends the 4-byte big-endian length prefix.
// Returns an empty array if the message exceeds kMaxFrameSize.
QByteArray encodeFrame(const pb::Envelope& envelope);

// Incremental decoder for a stream of length-prefixed Envelopes.
//
// Bytes may arrive in arbitrary pieces: a frame can be split anywhere
// (including inside the length prefix), and one piece can hold many frames.
// Feed the bytes with append() and call next() until it stops returning
// Result::Frame.
class FrameDecoder
{
public:
    enum class Result {
        Frame,        // a complete Envelope was written to the output
        NeedMoreData, // no complete frame is buffered yet
        Error         // the stream is corrupt; the connection must be closed
    };

    explicit FrameDecoder(quint32 maxFrameSize = kMaxFrameSize);

    void append(const QByteArray& bytes);
    void append(const char* data, int size);

    Result next(pb::Envelope& envelope);

    // Drops buffered bytes and clears the error state (for a new connection).
    void reset();

    bool hasFailed() const { return m_failed; }
    QString errorString() const { return m_error; }
    int bufferedBytes() const { return m_buffer.size() - m_readPos; }

private:
    Result fail(const QString& error);
    void compact();

    quint32 m_maxFrameSize;
    QByteArray m_buffer;
    int m_readPos = 0;
    bool m_failed = false;
    QString m_error;
};

} // namespace fm
