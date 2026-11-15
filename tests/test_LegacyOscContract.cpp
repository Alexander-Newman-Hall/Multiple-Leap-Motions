// Native socket receiver and OSC parser are independent of oscpack decoding.
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <gtest/gtest.h>
#include "pipeline/03_DataProcessor.hpp"
#include "pipeline/04_OscSender.hpp"
#include <bit>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {
class Receiver {
public:
    Receiver() {
#ifdef _WIN32
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("WSAStartup failed");
#endif
        socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_ == invalidSocket) throw std::runtime_error("receiver socket failed");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0; // OS-assigned port avoids test/process collisions.
        if (::bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
            throw std::runtime_error("receiver bind failed");
#ifdef _WIN32
        int size = sizeof(address);
#else
        socklen_t size = sizeof(address);
#endif
        if (getsockname(socket_, reinterpret_cast<sockaddr*>(&address), &size) != 0)
            throw std::runtime_error("getsockname failed");
        port_ = ntohs(address.sin_port);
    }
    ~Receiver() {
#ifdef _WIN32
        closesocket(socket_);
        WSACleanup();
#else
        ::close(socket_);
#endif
    }
    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;
    int port() const { return port_; }
    std::vector<unsigned char> receive() {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(socket_, &reads);
        timeval timeout{0, 250000};
        if (select(static_cast<int>(socket_ + 1), &reads, nullptr, nullptr, &timeout) != 1)
            throw std::runtime_error("Timed out waiting for UDP packet");
        char bytes[8192];
        const auto size = recv(socket_, bytes, sizeof(bytes), 0);
        if (size <= 0) throw std::runtime_error("UDP receive failed");
        return std::vector<unsigned char>(bytes, bytes + size);
    }
private:
#ifdef _WIN32
    using Socket = SOCKET;
    static constexpr Socket invalidSocket = INVALID_SOCKET;
#else
    using Socket = int;
    static constexpr Socket invalidSocket = -1;
#endif
    Socket socket_ = invalidSocket;
    int port_ = 0;
};

nlohmann::json decodeScalar(const std::vector<unsigned char>& bytes) {
    size_t offset = 0;
    auto readString = [&]() {
        const auto begin = offset;
        while (offset < bytes.size() && bytes[offset] != 0) ++offset;
        if (offset == bytes.size()) throw std::runtime_error("OSC missing NUL");
        std::string result(bytes.begin() + begin, bytes.begin() + offset);
        do {
            if (offset >= bytes.size() || bytes[offset] != 0) throw std::runtime_error("OSC invalid padding");
            ++offset;
        } while (offset % 4 != 0);
        return result;
    };
    const auto address = readString();
    const auto tags = readString();
    if (tags != ",f" || offset + 4 != bytes.size()) throw std::runtime_error("Expected one float32 argument");
    const std::uint32_t bits = (std::uint32_t(bytes[offset]) << 24) |
        (std::uint32_t(bytes[offset + 1]) << 16) | (std::uint32_t(bytes[offset + 2]) << 8) | bytes[offset + 3];
    return {{"address", address}, {"type", tags}, {"value", std::bit_cast<float>(bits)}};
}

nlohmann::json fixture(const char* name) {
    const auto path = std::filesystem::path(__FILE__).parent_path() / "fixtures" / "osc" / name;
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Missing fixture " + path.string());
    return nlohmann::json::parse(input);
}

FrameData sample(const std::string& serial = "serialA", const std::string& side = "left") {
    FrameData frame;
    frame.deviceId = serial;
    frame.timestamp = 1000000;
    HandData hand;
    hand.handType = side;
    hand.palm.position = {10, 20, -30};
    hand.arm.wristPosition = {40, 50, 60};
    hand.pinchStrength = .25f;
    hand.grabStrength = .75f;
    hand.palm.orientation = {.5f, -.5f, .25f, -.25f};
    hand.palm.velocity = {1, 2, 3};
    hand.palm.normal = {-1, 0, 1};
    hand.visibleTime = 2500000;
    for (int i = 0; i < 5; ++i) {
        FingerData finger;
        finger.fingerId = i;
        finger.isExtended = i % 2 == 0;
        finger.bones.resize(4);
        finger.bones[3].nextJoint = {float(100 + i), float(200 + i), float(300 + i)};
        hand.fingers.push_back(finger);
    }
    frame.hands.push_back(hand);
    return frame;
}
}

TEST(LegacyOscContract, DefaultActualProcessorAndUdpMatchFullGolden) {
    Receiver receiver;
    OscSender sender("127.0.0.1", receiver.port());
    ASSERT_TRUE(sender.isInitialized());
    DeviceAliasManager aliases;
    nlohmann::json actual = nlohmann::json::array();
    DataProcessor processor(aliases, [&](const OscMessage& message) {
        ASSERT_TRUE(sender.sendOscMessageChecked(message));
        actual.push_back(decodeScalar(receiver.receive()));
    }, nullptr, nullptr);
    processor.processData("serialA", sample());
    EXPECT_EQ(actual, fixture("default_hand.json"));
    EXPECT_EQ(sender.getStats().packetsSent, 23U);
    EXPECT_EQ(sender.getStats().sendErrors, 0U);
}

TEST(LegacyOscContract, AllOptionalChannelsMatchFullGolden) {
    Receiver receiver;
    OscSender sender("127.0.0.1", receiver.port());
    DeviceAliasManager aliases;
    nlohmann::json actual = nlohmann::json::array();
    DataProcessor processor(aliases, [&](const OscMessage& message) {
        ASSERT_TRUE(sender.sendOscMessageChecked(message));
        actual.push_back(decodeScalar(receiver.receive()));
    }, nullptr, nullptr);
    processor.setFilterSettings(true,true,true,true,true,true,true,true,true,true,true,true,true,true);
    processor.processData("serialA", sample());
    EXPECT_EQ(actual, fixture("all_channels_hand.json"));
    EXPECT_EQ(actual.size(), 39U);
}

TEST(LegacyOscContract, LossClearsOnceWithLegacyExistsQuirkAndDevicesStayIndependent) {
    Receiver receiver;
    OscSender sender("127.0.0.1", receiver.port());
    DeviceAliasManager aliases;
    nlohmann::json actual = nlohmann::json::array();
    DataProcessor processor(aliases, [&](const OscMessage& message) {
        ASSERT_TRUE(sender.sendOscMessageChecked(message));
        actual.push_back(decodeScalar(receiver.receive()));
    }, nullptr, nullptr);
    processor.processData("serialA", sample());
    processor.processData("serialB", sample("serialB", "right"));
    actual.clear();
    FrameData empty;
    processor.processData("serialA", empty);
    EXPECT_EQ(actual, fixture("default_hand_loss.json"));
    actual.clear();
    processor.processData("serialA", empty);
    EXPECT_TRUE(actual.empty()); // no repeated disappearance output
    processor.processData("serialB", sample("serialB", "right"));
    ASSERT_EQ(actual.size(), 23U);
    for (const auto& message : actual) EXPECT_EQ(message["address"].get<std::string>().find("/leap/dev2/right/"), 0U);
    actual.clear();
    processor.processData("serialA", sample());
    EXPECT_EQ(actual, fixture("default_hand.json"));
}

TEST(LegacyOscContract, ChannelSelectionAndOptionalLossQuirksArePreserved) {
    DeviceAliasManager aliases;
    std::vector<OscMessage> actual;
    DataProcessor processor(aliases, [&](const OscMessage& message) { actual.push_back(message); }, nullptr, nullptr);
    processor.setFilterSettings(false,false,false,true,false,false,false,false,false,false,false,false,false,false);
    processor.processData("serialA", sample());
    ASSERT_EQ(actual.size(), 3U);
    EXPECT_EQ(actual[0].address, "/leap/dev1/left/finger/index/tx");
    EXPECT_EQ(actual[0].values[0], 101.f);
    actual.clear();
    processor.setFilterSettings(false,false,false,false,false,false,false,true,true,true,false,true,false,false);
    processor.processData("serialA", sample());
    ASSERT_EQ(actual.size(), 15U); // extension independent of fingertip-position toggles
    actual.clear();
    processor.processData("serialA", FrameData{});
    EXPECT_TRUE(actual.empty()); // legacy does not clear orientation/velocity/normal/extension here
    processor.setFilterSettings(false,false,false,false,false,false,false,false,false,false,false,false,false,false);
    processor.processData("serialA", sample());
    EXPECT_TRUE(actual.empty());
}

TEST(LegacyOscContract, InvalidArmAndFingerAreOmitted) {
    DeviceAliasManager aliases;
    std::vector<OscMessage> actual;
    DataProcessor processor(aliases, [&](const OscMessage& message) { actual.push_back(message); }, nullptr, nullptr);
    auto frame = sample();
    frame.hands[0].arm.setValid(false);
    frame.hands[0].fingers[1].bones[3].setValid(false);
    processor.processData("serialA", frame);
    EXPECT_EQ(actual.size(), 17U);
    for (const auto& message : actual) {
        EXPECT_EQ(message.address.find("/wrist/"), std::string::npos);
        EXPECT_EQ(message.address.find("/finger/index/"), std::string::npos);
    }
}

TEST(LegacyOscContract, TransportFailureIsCountedAndExplicitResetRecovers) {
    Receiver receiver;
    OscSender sender("127.0.0.1", 0);
    EXPECT_FALSE(sender.isInitialized());
    EXPECT_FALSE(sender.sendMessageChecked("/probe", 1));
    EXPECT_EQ(sender.getStats().sendErrors, 1U);
    sender.setHost("127.0.0.1", receiver.port());
    EXPECT_TRUE(sender.sendMessageChecked("/probe", 2));
    const auto decoded = decodeScalar(receiver.receive());
    EXPECT_EQ(decoded["value"], 2);
    sender.close();
    EXPECT_FALSE(sender.sendMessageChecked("/probe", 3));
    EXPECT_EQ(sender.getStats().sendAttempts, 3U);
    EXPECT_EQ(sender.getStats().packetsSent, 1U);
    EXPECT_EQ(sender.getStats().sendErrors, 2U);
    EXPECT_FALSE(sender.getStats().lastError.empty());
}

TEST(LegacyOscContract, TargetChangeAndMultipleValuesKeepSeparatePackets) {
    Receiver first;
    Receiver second;
    OscSender sender("127.0.0.1", first.port());
    ASSERT_TRUE(sender.sendOscMessageChecked({"/probe", {1, 2}}));
    EXPECT_EQ(decodeScalar(first.receive())["value"], 1);
    EXPECT_EQ(decodeScalar(first.receive())["value"], 2);
    sender.updateTarget("127.0.0.1", second.port());
    ASSERT_TRUE(sender.sendMessageChecked("/next", 3));
    EXPECT_EQ(decodeScalar(second.receive())["address"], "/next");
    EXPECT_EQ(sender.getHost(), "127.0.0.1");
    EXPECT_EQ(sender.getPort(), second.port());
    EXPECT_EQ(sender.getStats().packetsSent, 3U);
    EXPECT_GT(sender.getStats().lastSuccessUs, 0U);
}

TEST(LegacyOscContract, NativeBroadcastPermissionErrorIsNotReportedAsSuccess) {
#ifdef _WIN32
    GTEST_SKIP() << "This kernel-permission regression is POSIX-specific: the Windows CI UDP provider accepts broadcast sends. "
                   "Readiness/closed-socket failures and actual UDP packet decoding are tested separately on Windows.";
#else
    // No SO_BROADCAST is enabled. The kernel rejects connect or send; no
    // broadcast packet leaves the socket. This exercises a real OS error.
    OscSender sender("255.255.255.255", 9000);
    EXPECT_FALSE(sender.sendMessageChecked("/probe", 1));
    const auto stats = sender.getStats();
    EXPECT_EQ(stats.sendAttempts, 1U);
    EXPECT_EQ(stats.packetsSent, 0U);
    EXPECT_EQ(stats.sendErrors, 1U);
    EXPECT_EQ(stats.lastSuccessUs, 0U);
    EXPECT_FALSE(stats.lastError.empty());
    EXPECT_EQ(stats.lastError.find("OSC target requires"), std::string::npos);
#endif
}
