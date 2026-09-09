// =============================================================================
// ClassroomMonitor — Protocol Unit Tests
// =============================================================================

#include "common/Protocol.h"
#include "common/NetworkTypes.h"
#include "common/VideoTypes.h"

#include <cassert>
#include <iostream>
#include <cstring>

using namespace cm;

void testPacketHeaderSize() {
    assert(sizeof(PacketHeader) == 16);
    std::cout << "[PASS] PacketHeader size = 16 bytes" << std::endl;
}

void testMakeAndParsePacket() {
    // Создаём пакет HANDSHAKE
    HandshakePayload hs;
    strncpy_s(hs.hostname, "STUDENT-PC-01", MAX_HOSTNAME_LEN);
    strncpy_s(hs.username, "student", MAX_USERNAME_LEN);
    hs.screenWidth  = 1920;
    hs.screenHeight = 1080;

    auto packet = makePacket(PacketType::HANDSHAKE, hs, 42);

    // Парсим заголовок
    PacketHeader header;
    bool ok = parseHeader(packet.data(), packet.size(), header);
    assert(ok);
    assert(header.magic == PROTOCOL_MAGIC);
    assert(header.type == static_cast<uint16_t>(PacketType::HANDSHAKE));
    assert(header.sequence == 42);
    assert(header.payloadSize == sizeof(HandshakePayload));

    // Парсим payload
    HandshakePayload parsedHs;
    ok = parsePayload(packet.data(), packet.size(), parsedHs);
    assert(ok);
    assert(std::string(parsedHs.hostname) == "STUDENT-PC-01");
    assert(std::string(parsedHs.username) == "student");
    assert(parsedHs.screenWidth == 1920);
    assert(parsedHs.screenHeight == 1080);

    std::cout << "[PASS] makePacket / parseHeader / parsePayload" << std::endl;
}

void testVideoPacket() {
    VideoFrameHeader frameHeader;
    frameHeader.clientId  = 1;
    frameHeader.timestamp = 123456789;
    frameHeader.width     = 1920;
    frameHeader.height    = 1080;
    frameHeader.frameType = 0; // I-frame
    frameHeader.quality   = 50;

    // Фиктивные данные кадра
    std::vector<uint8_t> fakeFrame(100, 0xAB);

    auto packet = makeVideoPacket(frameHeader, fakeFrame.data(), fakeFrame.size(), 7);

    // Парсим
    PacketHeader header;
    bool ok = parseHeader(packet.data(), packet.size(), header);
    assert(ok);
    assert(header.type == static_cast<uint16_t>(PacketType::VIDEO_FRAME));
    assert(header.sequence == 7);
    assert(header.payloadSize == sizeof(VideoFrameHeader) + 100);

    std::cout << "[PASS] makeVideoPacket" << std::endl;
}

void testInvalidPacket() {
    uint8_t garbage[] = { 0x00, 0x01, 0x02, 0x03 };
    PacketHeader header;
    bool ok = parseHeader(garbage, sizeof(garbage), header);
    assert(!ok); // magic не совпадает

    std::cout << "[PASS] Invalid packet rejected" << std::endl;
}

void testMouseMovePacket() {
    MouseMovePayload payload;
    payload.normalizedX = 0.5f;
    payload.normalizedY = 0.75f;

    auto packet = makePacket(PacketType::MOUSE_MOVE, payload, 10);

    PacketHeader header;
    bool ok = parseHeader(packet.data(), packet.size(), header);
    assert(ok);
    assert(header.type == static_cast<uint16_t>(PacketType::MOUSE_MOVE));

    MouseMovePayload parsed;
    ok = parsePayload(packet.data(), packet.size(), parsed);
    assert(ok);
    assert(parsed.normalizedX == 0.5f);
    assert(parsed.normalizedY == 0.75f);

    std::cout << "[PASS] MouseMovePayload" << std::endl;
}

void testNetworkConstants() {
    assert(net::VIDEO_PORT == 9100);
    assert(net::COMMAND_PORT == 9101);
    assert(net::HEARTBEAT_INTERVAL_MS == 3000);
    assert(net::HEARTBEAT_TIMEOUT_MS == 10000);

    std::cout << "[PASS] Network constants" << std::endl;
}

int main() {
    std::cout << "=== ClassroomMonitor Protocol Tests ===" << std::endl;

    testPacketHeaderSize();
    testMakeAndParsePacket();
    testVideoPacket();
    testInvalidPacket();
    testMouseMovePacket();
    testNetworkConstants();

    std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
    return 0;
}
