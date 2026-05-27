#include "network/net_transport.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace splonks::network {

namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
using SocketResult = int;
using SendRecvSize = int;
using SockLen = int;
constexpr std::uintptr_t kInvalidSocket = static_cast<std::uintptr_t>(INVALID_SOCKET);

NativeSocket NativeHandle(std::uintptr_t handle) {
    return static_cast<NativeSocket>(handle);
}

bool EnsureWinsock(std::string* error_out) {
    static const int startup_result = []() {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data);
    }();
    if (startup_result != 0) {
        if (error_out != nullptr) {
            *error_out = "WSAStartup: winsock error " + std::to_string(startup_result);
        }
        return false;
    }
    return true;
}

std::string SocketErrorText() {
    return "winsock error " + std::to_string(WSAGetLastError());
}
#else
using NativeSocket = int;
using SocketResult = ssize_t;
using SendRecvSize = std::size_t;
using SockLen = socklen_t;
constexpr int kInvalidSocket = -1;

NativeSocket NativeHandle(int handle) {
    return handle;
}

bool EnsureWinsock(std::string*) {
    return true;
}

std::string SocketErrorText() {
    return std::strerror(errno);
}
#endif

sockaddr_in ToSockAddr(const NetEndpoint& endpoint) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint.port);
    if (inet_pton(AF_INET, endpoint.address.c_str(), &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    return addr;
}

NetEndpoint FromSockAddr(const sockaddr_in& addr) {
    std::array<char, INET_ADDRSTRLEN> buffer{};
    const char* text = inet_ntop(AF_INET, &addr.sin_addr, buffer.data(), static_cast<socklen_t>(buffer.size()));
    NetEndpoint endpoint;
    endpoint.address = text != nullptr ? text : "0.0.0.0";
    endpoint.port = ntohs(addr.sin_port);
    return endpoint;
}

void SetError(std::string* error_out, const std::string& context) {
    if (error_out != nullptr) {
        *error_out = context + ": " + SocketErrorText();
    }
}

} // namespace

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : fd_(other.fd_), bound_port_(other.bound_port_) {
    other.fd_ = kInvalidSocket;
    other.bound_port_ = 0;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    Close();
    fd_ = other.fd_;
    bound_port_ = other.bound_port_;
    other.fd_ = kInvalidSocket;
    other.bound_port_ = 0;
    return *this;
}

UdpSocket::~UdpSocket() {
    Close();
}

bool UdpSocket::Open(std::uint16_t bind_port, std::string* error_out) {
    Close();
    if (!EnsureWinsock(error_out)) {
        return false;
    }

    const NativeSocket opened_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (opened_socket == NativeHandle(kInvalidSocket)) {
        SetError(error_out, "socket");
        return false;
    }
    fd_ = static_cast<decltype(fd_)>(opened_socket);

    int reuse = 1;
    (void)setsockopt(
        NativeHandle(fd_),
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&reuse),
        sizeof(reuse)
    );

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(bind_port);
    if (bind(NativeHandle(fd_), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        SetError(error_out, "bind");
        Close();
        return false;
    }

#ifdef _WIN32
    u_long nonblocking = 1;
    if (ioctlsocket(NativeHandle(fd_), FIONBIO, &nonblocking) != 0) {
        SetError(error_out, "ioctlsocket");
        Close();
        return false;
    }
#else
    if (fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL, 0) | O_NONBLOCK) != 0) {
        SetError(error_out, "fcntl");
        Close();
        return false;
    }
#endif

    sockaddr_in bound_addr{};
    SockLen bound_len = sizeof(bound_addr);
    if (getsockname(NativeHandle(fd_), reinterpret_cast<sockaddr*>(&bound_addr), &bound_len) == 0) {
        bound_port_ = ntohs(bound_addr.sin_port);
    } else {
        bound_port_ = bind_port;
    }
    return true;
}

void UdpSocket::Close() {
    if (fd_ != kInvalidSocket) {
#ifdef _WIN32
        (void)closesocket(NativeHandle(fd_));
#else
        (void)close(fd_);
#endif
    }
    fd_ = kInvalidSocket;
    bound_port_ = 0;
}

bool UdpSocket::IsOpen() const {
    return fd_ != kInvalidSocket;
}

std::uint16_t UdpSocket::BoundPort() const {
    return bound_port_;
}

bool UdpSocket::Send(
    const NetEndpoint& endpoint,
    const std::uint8_t* bytes,
    std::size_t size,
    std::string* error_out
) {
    if (fd_ == kInvalidSocket) {
        if (error_out != nullptr) {
            *error_out = "socket is closed";
        }
        return false;
    }

    const sockaddr_in addr = ToSockAddr(endpoint);
    const SocketResult sent = sendto(
        NativeHandle(fd_),
        reinterpret_cast<const char*>(bytes),
        static_cast<SendRecvSize>(size),
        0,
        reinterpret_cast<const sockaddr*>(&addr),
        sizeof(addr)
    );
    if (sent < 0 || static_cast<std::size_t>(sent) != size) {
        SetError(error_out, "sendto");
        return false;
    }
    return true;
}

std::optional<UdpPacket> UdpSocket::Receive(std::string* error_out) {
    if (fd_ == kInvalidSocket) {
        return std::nullopt;
    }

    UdpPacket packet;
    sockaddr_in from{};
    SockLen from_len = sizeof(from);
    const SocketResult received = recvfrom(
        NativeHandle(fd_),
        reinterpret_cast<char*>(packet.bytes.data()),
        static_cast<SendRecvSize>(packet.bytes.size()),
        0,
        reinterpret_cast<sockaddr*>(&from),
        &from_len
    );
    if (received < 0) {
#ifdef _WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return std::nullopt;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return std::nullopt;
        }
#endif
        SetError(error_out, "recvfrom");
        return std::nullopt;
    }

    packet.endpoint = FromSockAddr(from);
    packet.size = static_cast<std::size_t>(received);
    return packet;
}

NetTransportRuntime NetTransportRuntime::New() {
    return NetTransportRuntime{};
}

bool EndpointsEqual(const NetEndpoint& a, const NetEndpoint& b) {
    return a.address == b.address && a.port == b.port;
}

std::string EndpointToString(const NetEndpoint& endpoint) {
    return endpoint.address + ":" + std::to_string(endpoint.port);
}

std::vector<std::string> GetLocalLanIpv4Addresses() {
    std::vector<std::string> addresses;

#ifdef _WIN32
    if (!EnsureWinsock(nullptr)) {
        return addresses;
    }

    std::array<char, 256> hostname{};
    if (gethostname(hostname.data(), static_cast<int>(hostname.size())) != 0) {
        return addresses;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo* results = nullptr;
    if (getaddrinfo(hostname.data(), nullptr, &hints, &results) != 0) {
        return addresses;
    }

    for (const addrinfo* result = results; result != nullptr; result = result->ai_next) {
        const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(result->ai_addr);
        std::array<char, INET_ADDRSTRLEN> buffer{};
        const char* text = inet_ntop(AF_INET, &addr->sin_addr, buffer.data(), static_cast<SockLen>(buffer.size()));
        if (text == nullptr) {
            continue;
        }

        const std::string address = text;
        if (address.rfind("127.", 0) == 0) {
            continue;
        }
        if (std::find(addresses.begin(), addresses.end(), address) == addresses.end()) {
            addresses.push_back(address);
        }
    }
    freeaddrinfo(results);
#else
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0) {
        return addresses;
    }

    for (const ifaddrs* iface = interfaces; iface != nullptr; iface = iface->ifa_next) {
        if (iface->ifa_addr == nullptr || iface->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if ((iface->ifa_flags & IFF_UP) == 0 || (iface->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }

        const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(iface->ifa_addr);
        std::array<char, INET_ADDRSTRLEN> buffer{};
        const char* text = inet_ntop(
            AF_INET,
            &addr->sin_addr,
            buffer.data(),
            static_cast<socklen_t>(buffer.size())
        );
        if (text == nullptr) {
            continue;
        }

        const std::string address = text;
        if (std::find(addresses.begin(), addresses.end(), address) == addresses.end()) {
            addresses.push_back(address);
        }
    }

    freeifaddrs(interfaces);
#endif
    return addresses;
}

} // namespace splonks::network
