#include "common/FrameCodec.h"

#include <QtEndian>

namespace fm {

QByteArray encodeFrame(const pb::Envelope& envelope)
{
    const size_t bodySize = envelope.ByteSizeLong();
    if (bodySize > kMaxFrameSize)
        return QByteArray();

    QByteArray frame(kFrameHeaderSize + static_cast<int>(bodySize), Qt::Uninitialized);
    qToBigEndian<quint32>(static_cast<quint32>(bodySize), frame.data());
    if (!envelope.SerializeToArray(frame.data() + kFrameHeaderSize, static_cast<int>(bodySize)))
        return QByteArray();
    return frame;
}

FrameDecoder::FrameDecoder(quint32 maxFrameSize)
    : m_maxFrameSize(maxFrameSize)
{
}

void FrameDecoder::append(const QByteArray& bytes)
{
    append(bytes.constData(), bytes.size());
}

void FrameDecoder::append(const char* data, int size)
{
    if (m_failed || size <= 0)
        return;
    m_buffer.append(data, size);
}

FrameDecoder::Result FrameDecoder::next(pb::Envelope& envelope)
{
    if (m_failed)
        return Result::Error;

    const int available = m_buffer.size() - m_readPos;
    if (available < kFrameHeaderSize) {
        compact();
        return Result::NeedMoreData;
    }

    const char* frame = m_buffer.constData() + m_readPos;
    const quint32 bodySize = qFromBigEndian<quint32>(frame);
    if (bodySize > m_maxFrameSize) {
        return fail(QStringLiteral("frame of %1 bytes exceeds the limit of %2 bytes")
                        .arg(bodySize)
                        .arg(m_maxFrameSize));
    }

    if (static_cast<quint32>(available - kFrameHeaderSize) < bodySize) {
        compact();
        return Result::NeedMoreData;
    }

    if (!envelope.ParseFromArray(frame + kFrameHeaderSize, static_cast<int>(bodySize)))
        return fail(QStringLiteral("frame payload is not a valid Envelope"));

    m_readPos += kFrameHeaderSize + static_cast<int>(bodySize);
    if (m_readPos == m_buffer.size()) {
        m_buffer.clear();
        m_readPos = 0;
    }
    return Result::Frame;
}

void FrameDecoder::reset()
{
    m_buffer.clear();
    m_readPos = 0;
    m_failed = false;
    m_error.clear();
}

FrameDecoder::Result FrameDecoder::fail(const QString& error)
{
    m_failed = true;
    m_error = error;
    m_buffer.clear();
    m_readPos = 0;
    return Result::Error;
}

// Consumed bytes are dropped only when we run out of complete frames, so a
// burst of many small frames is not re-copied once per frame.
void FrameDecoder::compact()
{
    if (m_readPos == 0)
        return;
    m_buffer.remove(0, m_readPos);
    m_readPos = 0;
}

} // namespace fm
