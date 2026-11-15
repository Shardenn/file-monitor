#pragma once

#include <QtGlobal>

#include "filemonitor.pb.h"

namespace fm {

// Short alias for the generated protobuf namespace.
namespace pb = ::filemonitor::proto;

constexpr quint32 kProtocolVersion = 1;
constexpr quint16 kDefaultPort = 5555;

// Size of the big-endian length prefix that precedes every Envelope.
constexpr int kFrameHeaderSize = 4;

// Upper bound for a single serialized Envelope. A larger length prefix is
// treated as a protocol error, so a broken peer cannot make us allocate
// gigabytes.
constexpr quint32 kMaxFrameSize = 4u * 1024u * 1024u;

// Files up to this size are sent as one chunk; larger files are split.
constexpr qint64 kSingleChunkLimit = 1024 * 1024;
constexpr qint64 kFileChunkSize = 256 * 1024;

// The file sender stops reading the file while the socket has this many
// bytes queued, which bounds memory use per transfer.
constexpr qint64 kSendHighWaterMark = 1024 * 1024;

} // namespace fm
