// =============================================================================
// ClassroomMonitor — Protocol Implementation
// =============================================================================

#include "common/Protocol.h"
#include <cstring>

namespace cm {

std::vector<uint8_t> makeVideoPacket(const VideoFrameHeader& frameHeader,
                                      const uint8_t* frameData,
                                      size_t frameSize,
                                      uint32_t seq) {
    PacketHeader header;
    header.type        = static_cast<uint16_t>(PacketType::VIDEO_FRAME);
    header.sequence    = seq;
    header.payloadSize = static_cast<uint32_t>(sizeof(VideoFrameHeader) + frameSize);

    const size_t totalSize = sizeof(PacketHeader) + sizeof(VideoFrameHeader) + frameSize;
    std::vector<uint8_t> buffer(totalSize);

    size_t offset = 0;

    // Заголовок пакета
    std::memcpy(buffer.data() + offset, &header, sizeof(PacketHeader));
    offset += sizeof(PacketHeader);

    // Заголовок видеокадра
    std::memcpy(buffer.data() + offset, &frameHeader, sizeof(VideoFrameHeader));
    offset += sizeof(VideoFrameHeader);

    // Данные кадра (H.264)
    if (frameData && frameSize > 0) {
        std::memcpy(buffer.data() + offset, frameData, frameSize);
    }

    return buffer;
}

bool parseHeader(const uint8_t* data, size_t size, PacketHeader& outHeader) {
    if (!data || size < sizeof(PacketHeader)) {
        return false;
    }

    std::memcpy(&outHeader, data, sizeof(PacketHeader));

    // Проверяем magic number
    if (outHeader.magic != PROTOCOL_MAGIC) {
        return false;
    }

    return true;
}

} // namespace cm
