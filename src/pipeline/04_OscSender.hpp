#pragma once
// Renamed: 04_OscSender
#include <string>
#include <vector>
#include <map>
#include "transport/osc/OscMessage.hpp"
#include "core/interfaces/ITransportSink.hpp"
#include <memory>
#include <mutex>
#include <cstdint>
#include <span>

// Native UDP transport checks errors; oscpack remains the OSC serializer.
class HandbridgeUdpSocket;

class OscSender : public ITransportSink {
public:
    // Successful sends mean OS acceptance only, never receiver acknowledgement.
    struct Stats {
        std::uint64_t sendAttempts = 0;
        std::uint64_t packetsSent = 0;
        std::uint64_t bytesSent = 0;
        std::uint64_t sendErrors = 0;
        std::uint64_t lastAttemptUs = 0;
        std::uint64_t lastSuccessUs = 0;
        std::string lastError;
    };
    OscSender(const std::string& host, int port);
    OscSender(const char* ip, int port);
    ~OscSender();

    void setHost(const std::string& host, int port);
    bool isInitialized() const;
    void sendMessage(const std::string& address, float value);
    bool sendMessageChecked(const std::string& address, float value);
    bool sendOscMessageChecked(const OscMessage& message);
    Stats getStats() const;
    void sendMessages(const std::vector<OscMessage>& messages);
    void sendBundle(const std::map<std::string, float>& messages, const std::string& baseAddress = "");

    // ITransportSink interface
    bool send(const void* data, size_t size) override;
    void sendOscMessage(const OscMessage& message) override;
    void updateTarget(const std::string& target, int port) override;
    void close() override;

public:
    std::string getHost() const;
    int getPort() const;
private:
    bool sendArgumentsChecked(const std::string& address, std::span<const OscArgument> arguments);
    // Socket creation, packet buffer and status have one owner at a time.
    // Caller holds mutex_ for the following internal helpers.
    void initializeSocket();
    bool sendPacket(const void* data, size_t size);
    void recordError(const std::string& message);
    mutable std::mutex mutex_;
    Stats stats_;
    bool errorActive_ = false;
    std::uint64_t lastErrorLogUs_ = 0;
    std::string socketUnavailableReason_;
    std::string host_;
    int port_;
    // Use unique_ptr for the socket
    std::unique_ptr<HandbridgeUdpSocket> socket_;
    static constexpr size_t OUTPUT_BUFFER_SIZE = 4096;
    std::vector<char> buffer_;
};
