#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "04_OscSender.hpp"
#include <osc/OscOutboundPacketStream.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <system_error>

// oscpack's UdpSocket::Send discards send()'s return value on both platforms.
// Keep its wire serializer, but check actual OS results here.
class HandbridgeUdpSocket {
public:
    HandbridgeUdpSocket(const std::string& host, int port) {
        try {
#ifdef _WIN32
            WSADATA data{};
            const auto startup = WSAStartup(MAKEWORD(2, 2), &data);
            if (startup != 0) throw std::system_error(startup, std::system_category(), "WSAStartup");
            winsockStarted_ = true;
#endif
            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            addrinfo* rawAddress = nullptr;
            const auto service = std::to_string(port);
            const int result = getaddrinfo(host.c_str(), service.c_str(), &hints, &rawAddress);
            if (result != 0) throw std::runtime_error("Cannot resolve OSC host (getaddrinfo code " + std::to_string(result) + ")");
            std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> address(rawAddress, freeaddrinfo);
            socket_ = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
            if (socket_ == invalidSocket) throwSocketError("socket");
            if (::connect(socket_, address->ai_addr, static_cast<int>(address->ai_addrlen)) != 0)
                throwSocketError("connect");
#ifdef _WIN32
            u_long nonblocking = 1;
            if (ioctlsocket(socket_, FIONBIO, &nonblocking) != 0) throwSocketError("ioctlsocket");
#else
            const int flags = fcntl(socket_, F_GETFL, 0);
            if (flags < 0 || fcntl(socket_, F_SETFL, flags | O_NONBLOCK) != 0) throwSocketError("fcntl");
#endif
        } catch (...) {
            cleanup();
            throw;
        }
    }
    ~HandbridgeUdpSocket() { cleanup(); }
    HandbridgeUdpSocket(const HandbridgeUdpSocket&) = delete;
    HandbridgeUdpSocket& operator=(const HandbridgeUdpSocket&) = delete;

    void Send(const char* bytes, int size) {
        const auto sent = ::send(socket_, bytes, size, 0);
        if (sent < 0) throwSocketError("send");
        if (sent != size) throw std::runtime_error("UDP send returned an incomplete datagram");
    }
private:
    [[noreturn]] static void throwSocketError(const char* operation) {
#ifdef _WIN32
        const auto error = WSAGetLastError();
#else
        const auto error = errno;
#endif
        throw std::system_error(error, std::system_category(), operation);
    }
    void cleanup() {
        if (socket_ != invalidSocket) {
#ifdef _WIN32
            closesocket(socket_);
#else
            ::close(socket_);
#endif
            socket_ = invalidSocket;
        }
#ifdef _WIN32
        if (winsockStarted_) WSACleanup();
        winsockStarted_ = false;
#endif
    }
#ifdef _WIN32
    using Socket = SOCKET;
    static constexpr Socket invalidSocket = INVALID_SOCKET;
    bool winsockStarted_ = false;
#else
    using Socket = int;
    static constexpr Socket invalidSocket = -1;
#endif
    Socket socket_ = invalidSocket;
};

namespace {
std::uint64_t monotonicUs() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
}

OscSender::OscSender(const std::string& host, int port)
    : host_(host), port_(port), buffer_(OUTPUT_BUFFER_SIZE) {
    initializeSocket();
}

OscSender::OscSender(const char* ip, int port)
    : OscSender(std::string(ip), port) {}

OscSender::~OscSender() = default;

void OscSender::recordError(const std::string& message) {
    stats_.lastError = message;
    // Count every failure. Connected UDP can alternate OS success/ICMP errors
    // when no receiver listens; cap those repeated transitions to one log/sec.
    const auto now = monotonicUs();
    if (!errorActive_ && (lastErrorLogUs_ == 0 || now - lastErrorLogUs_ >= 1000000)) {
        std::cerr << "[OscSender] " << message << '\n';
        lastErrorLogUs_ = now;
    }
    errorActive_ = true;
}

bool OscSender::sendPacket(const void* data, size_t size) {
    if (!socket_) {
        recordError(socketUnavailableReason_.empty() ? "Socket is not initialized" : socketUnavailableReason_);
        ++stats_.sendErrors;
        return false;
    }
    if (!data || size == 0 || size > 65507) {
        recordError("Invalid UDP payload size");
        ++stats_.sendErrors;
        return false;
    }
    try {
        socket_->Send(static_cast<const char*>(data), static_cast<int>(size));
        ++stats_.packetsSent;
        stats_.bytesSent += size;
        stats_.lastSuccessUs = monotonicUs();
        errorActive_ = false;
        return true;
    } catch (const std::exception& e) {
        ++stats_.sendErrors;
        recordError(e.what());
        return false;
    }
}

bool OscSender::send(const void* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.sendAttempts;
    stats_.lastAttemptUs = monotonicUs();
    return sendPacket(data, size);
}

void OscSender::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    socket_.reset();
    socketUnavailableReason_ = "Socket was closed";
}

void OscSender::initializeSocket() {
    socket_.reset();
    try {
        if (host_.empty() || port_ < 1 || port_ > 65535)
            throw std::invalid_argument("OSC target requires a host and UDP port 1..65535");
        socket_ = std::make_unique<HandbridgeUdpSocket>(host_, port_);
        socketUnavailableReason_.clear();
        errorActive_ = false;
    } catch (const std::exception& e) {
        socketUnavailableReason_ = e.what();
        recordError(e.what());
    }
}

void OscSender::setHost(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(mutex_);
    host_ = host;
    port_ = port;
    initializeSocket();
}

bool OscSender::isInitialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return socket_ != nullptr;
}

void OscSender::sendBundle(const std::map<std::string, float>&, const std::string&) {
    // Deliberately not implemented. Keep default legacy one-scalar packets.
    std::lock_guard<std::mutex> lock(mutex_);
    recordError("sendBundle is not implemented; use legacy scalar sends");
}

bool OscSender::sendMessageChecked(const std::string& address, float value) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.sendAttempts;
    stats_.lastAttemptUs = monotonicUs();
    try {
        osc::OutboundPacketStream packet(buffer_.data(), buffer_.size());
        packet << osc::BeginMessage(address.c_str()) << value << osc::EndMessage;
        return sendPacket(packet.Data(), packet.Size());
    } catch (const std::exception& e) {
        ++stats_.sendErrors;
        recordError(e.what());
        return false;
    }
}

void OscSender::sendMessage(const std::string& address, float value) {
    (void)sendMessageChecked(address, value);
}

bool OscSender::sendOscMessageChecked(const OscMessage& message) {
    if (message.address.empty() || message.values.empty()) return false;
    bool success = true;
    // Legacy behavior: even multiple values produce separate scalar datagrams.
    for (float value : message.values) {
        if (!sendMessageChecked(message.address, value)) success = false;
    }
    return success;
}

void OscSender::sendMessages(const std::vector<OscMessage>& messages) {
    for (const auto& message : messages) (void)sendOscMessageChecked(message);
}

void OscSender::sendOscMessage(const OscMessage& message) {
    (void)sendOscMessageChecked(message);
}

void OscSender::updateTarget(const std::string& target, int port) {
    setHost(target, port);
}

OscSender::Stats OscSender::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

std::string OscSender::getHost() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return host_;
}

int OscSender::getPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return port_;
}
