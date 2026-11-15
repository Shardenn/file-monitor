#include <gtest/gtest.h>

#include <QtEndian>

#include "common/FrameCodec.h"

using fm::FrameDecoder;
namespace pb = fm::pb;

namespace {

pb::Envelope makeEvent(const std::string& name, quint64 size = 42)
{
    pb::Envelope envelope;
    pb::FileEvent* event = envelope.mutable_file_event();
    event->set_type(pb::FILE_MODIFIED);
    event->set_file_name(name);
    event->set_size(size);
    return envelope;
}

QByteArray lengthPrefix(quint32 length)
{
    QByteArray prefix(4, Qt::Uninitialized);
    qToBigEndian<quint32>(length, prefix.data());
    return prefix;
}

} // namespace

TEST(FrameCodec, PrefixIsBigEndianBodyLength)
{
    const pb::Envelope envelope = makeEvent("a.txt");
    const QByteArray frame = fm::encodeFrame(envelope);

    ASSERT_GT(frame.size(), 4);
    const quint32 bodySize = static_cast<quint32>(frame.size() - 4);
    EXPECT_EQ(static_cast<quint8>(frame[0]), (bodySize >> 24) & 0xff);
    EXPECT_EQ(static_cast<quint8>(frame[1]), (bodySize >> 16) & 0xff);
    EXPECT_EQ(static_cast<quint8>(frame[2]), (bodySize >> 8) & 0xff);
    EXPECT_EQ(static_cast<quint8>(frame[3]), bodySize & 0xff);
    EXPECT_EQ(static_cast<size_t>(bodySize), envelope.ByteSizeLong());
}

TEST(FrameCodec, RefusesToEncodeOversizedMessage)
{
    pb::Envelope envelope;
    envelope.mutable_file_chunk()->set_data(std::string(fm::kMaxFrameSize + 1, 'x'));
    EXPECT_TRUE(fm::encodeFrame(envelope).isEmpty());
}

TEST(FrameDecoder, DecodesSingleFrame)
{
    FrameDecoder decoder;
    decoder.append(fm::encodeFrame(makeEvent("a.txt", 7)));

    pb::Envelope out;
    ASSERT_EQ(decoder.next(out), FrameDecoder::Result::Frame);
    EXPECT_EQ(out.body_case(), pb::Envelope::kFileEvent);
    EXPECT_EQ(out.file_event().file_name(), "a.txt");
    EXPECT_EQ(out.file_event().size(), 7u);
    EXPECT_EQ(decoder.next(out), FrameDecoder::Result::NeedMoreData);
    EXPECT_EQ(decoder.bufferedBytes(), 0);
}

TEST(FrameDecoder, EmptyInputNeedsMoreData)
{
    FrameDecoder decoder;
    pb::Envelope out;
    EXPECT_EQ(decoder.next(out), FrameDecoder::Result::NeedMoreData);
    decoder.append(QByteArray());
    EXPECT_EQ(decoder.next(out), FrameDecoder::Result::NeedMoreData);
}

TEST(FrameDecoder, ReassemblesFrameFedByteByByte)
{
    const QByteArray frame = fm::encodeFrame(makeEvent("byte-by-byte.bin", 123456789));
    FrameDecoder decoder;
    pb::Envelope out;

    for (int i = 0; i < frame.size() - 1; ++i) {
        decoder.append(frame.constData() + i, 1);
        ASSERT_EQ(decoder.next(out), FrameDecoder::Result::NeedMoreData) << "at byte " << i;
    }
    decoder.append(frame.constData() + frame.size() - 1, 1);
    ASSERT_EQ(decoder.next(out), FrameDecoder::Result::Frame);
    EXPECT_EQ(out.file_event().file_name(), "byte-by-byte.bin");
    EXPECT_EQ(out.file_event().size(), 123456789u);
}

TEST(FrameDecoder, HandlesPrefixSplitAcrossReads)
{
    const QByteArray frame = fm::encodeFrame(makeEvent("split.txt"));
    FrameDecoder decoder;
    pb::Envelope out;

    decoder.append(frame.left(2)); // half of the length prefix
    EXPECT_EQ(decoder.next(out), FrameDecoder::Result::NeedMoreData);
    decoder.append(frame.mid(2, 3)); // rest of the prefix and one body byte
    EXPECT_EQ(decoder.next(out), FrameDecoder::Result::NeedMoreData);
    decoder.append(frame.mid(5));
    ASSERT_EQ(decoder.next(out), FrameDecoder::Result::Frame);
    EXPECT_EQ(out.file_event().file_name(), "split.txt");
}

TEST(FrameDecoder, DecodesSeveralFramesFromOneBuffer)
{
    QByteArray stream;
    for (int i = 0; i < 100; ++i)
        stream += fm::encodeFrame(makeEvent("file" + std::to_string(i), static_cast<quint64>(i)));

    FrameDecoder decoder;
    decoder.append(stream);

    pb::Envelope out;
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(decoder.next(out), FrameDecoder::Result::Frame) << "frame " << i;
        EXPECT_EQ(out.file_event().file_name(), "file" + std::to_string(i));
        EXPECT_EQ(out.file_event().size(), static_cast<quint64>(i));
    }
    EXPECT_EQ(decoder.next(out), FrameDecoder::Result::NeedMoreData);
}

TEST(FrameDecoder, KeepsTrailingPartialFrame)
{
    const QByteArray first = fm::encodeFrame(makeEvent("first"));
    const QByteArray second = fm::encodeFrame(makeEvent("second"));
    FrameDecoder decoder;
    pb::Envelope out;

    decoder.append(first + second.left(second.size() / 2));
    ASSERT_EQ(decoder.next(out), FrameDecoder::Result::Frame);
    EXPECT_EQ(out.file_event().file_name(), "first");
    EXPECT_EQ(decoder.next(out), FrameDecoder::Result::NeedMoreData);

    decoder.append(second.mid(second.size() / 2));
    ASSERT_EQ(decoder.next(out), FrameDecoder::Result::Frame);
    EXPECT_EQ(out.file_event().file_name(), "second");
}

// The stream is cut at every possible position, which covers every way TCP
// can fragment it.
TEST(FrameDecoder, AnySplitPointGivesTheSameMessages)
{
    const QByteArray stream = fm::encodeFrame(makeEvent("one", 1))
        + fm::encodeFrame(makeEvent("two", 2)) + fm::encodeFrame(makeEvent("three", 3));

    for (int split = 0; split <= stream.size(); ++split) {
        FrameDecoder decoder;
        std::vector<std::string> names;
        pb::Envelope out;

        decoder.append(stream.left(split));
        while (decoder.next(out) == FrameDecoder::Result::Frame)
            names.push_back(out.file_event().file_name());
        decoder.append(stream.mid(split));
        while (decoder.next(out) == FrameDecoder::Result::Frame)
            names.push_back(out.file_event().file_name());

        const std::vector<std::string> expected = {"one", "two", "three"};
        ASSERT_EQ(names, expected) << "split at " << split;
        ASSERT_FALSE(decoder.hasFailed());
    }
}

TEST(FrameDecoder, ZeroLengthFrameIsAnEmptyEnvelope)
{
    FrameDecoder decoder;
    decoder.append(lengthPrefix(0) + fm::encodeFrame(makeEvent("after")));

    pb::Envelope out;
    ASSERT_EQ(decoder.next(out), FrameDecoder::Result::Frame);
    EXPECT_EQ(out.body_case(), pb::Envelope::BODY_NOT_SET);
    ASSERT_EQ(decoder.next(out), FrameDecoder::Result::Frame);
    EXPECT_EQ(out.file_event().file_name(), "after");
}

TEST(FrameDecoder, RejectsOversizedLength)
{
    FrameDecoder decoder;
    decoder.append(lengthPrefix(fm::kMaxFrameSize + 1));

    pb::Envelope out;
    EXPECT_EQ(decoder.next(out), FrameDecoder::Result::Error);
    EXPECT_TRUE(decoder.hasFailed());
    EXPECT_FALSE(decoder.errorString().isEmpty());
}

TEST(FrameDecoder, AcceptsFrameOfExactlyTheMaximumSize)
{
    FrameDecoder decoder(16);
    decoder.append(lengthPrefix(17));
    pb::Envelope out;
    EXPECT_EQ(decoder.next(out), FrameDecoder::Result::Error);

    FrameDecoder small(64);
    const QByteArray frame = fm::encodeFrame(makeEvent("fits"));
    ASSERT_LE(frame.size() - 4, 64);
    small.append(frame);
    EXPECT_EQ(small.next(out), FrameDecoder::Result::Frame);
}

TEST(FrameDecoder, RejectsCorruptPayload)
{
    // Field 1, wire type 2 (length-delimited), declared length 100, but
    // only two bytes follow: not a valid protobuf message.
    const QByteArray garbage("\x0a\x64\x01\x02", 4);
    FrameDecoder decoder;
    decoder.append(lengthPrefix(static_cast<quint32>(garbage.size())) + garbage);

    pb::Envelope out;
    EXPECT_EQ(decoder.next(out), FrameDecoder::Result::Error);
}

TEST(FrameDecoder, StaysFailedUntilReset)
{
    FrameDecoder decoder;
    decoder.append(lengthPrefix(0xffffffffu));

    pb::Envelope out;
    ASSERT_EQ(decoder.next(out), FrameDecoder::Result::Error);

    // After an error the frame boundaries are unknown; later bytes must not
    // be interpreted.
    decoder.append(fm::encodeFrame(makeEvent("ignored")));
    EXPECT_EQ(decoder.next(out), FrameDecoder::Result::Error);

    decoder.reset();
    EXPECT_FALSE(decoder.hasFailed());
    decoder.append(fm::encodeFrame(makeEvent("fresh")));
    ASSERT_EQ(decoder.next(out), FrameDecoder::Result::Frame);
    EXPECT_EQ(out.file_event().file_name(), "fresh");
}

TEST(FrameCodec, RoundTripsEveryMessageKind)
{
    std::vector<pb::Envelope> messages(6);
    messages[0].mutable_hello()->set_protocol_version(fm::kProtocolVersion);
    messages[0].mutable_hello()->set_watched_directory("/srv/data");
    messages[0].mutable_hello()->set_monitoring_active(true);
    messages[1] = makeEvent("event.txt");
    messages[1].mutable_file_event()->set_sha256(std::string(32, '\x7f'));
    messages[2].mutable_command()->set_request_id(9);
    messages[2].mutable_command()->mutable_file_request()->set_file_name("wanted.txt");
    messages[3].mutable_command_result()->set_request_id(9);
    messages[3].mutable_command_result()->set_message("nope");
    messages[4].mutable_monitoring_state()->set_active(true);
    messages[5].mutable_file_chunk()->set_request_id(9);
    messages[5].mutable_file_chunk()->set_data(std::string("\0\x01\xff binary", 10));
    messages[5].mutable_file_chunk()->set_is_last(true);

    FrameDecoder decoder;
    for (const pb::Envelope& message : messages)
        decoder.append(fm::encodeFrame(message));

    for (const pb::Envelope& message : messages) {
        pb::Envelope out;
        ASSERT_EQ(decoder.next(out), FrameDecoder::Result::Frame);
        EXPECT_EQ(out.body_case(), message.body_case());
        EXPECT_EQ(out.SerializeAsString(), message.SerializeAsString());
    }
}
