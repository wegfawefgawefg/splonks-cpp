#include "network/net_transport.hpp"

#include <arpa/inet.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace splonks::network {

namespace {

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
        *error_out = context + ": " + std::strerror(errno);
    }
}

} // namespace

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : fd_(other.fd_), bound_port_(other.bound_port_) {
    other.fd_ = -1;
    other.bound_port_ = 0;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    Close();
    fd_ = other.fd_;
    bound_port_ = other.bound_port_;
    other.fd_ = -1;
    other.bound_port_ = 0;
    return *this;
}

UdpSocket::~UdpSocket() {
    Close();
}

bool UdpSocket::Open(std::uint16_t bind_port, std::string* error_out) {
    Close();

    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
        SetError(error_out, "socket");
        return false;
    }

    int reuse = 1;
    (void)setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(bind_port);
    if (bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        SetError(error_out, "bind");
        Close();
        return false;
    }

    if (fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL, 0) | O_NONBLOCK) != 0) {
        SetError(error_out, "fcntl");
        Close();
        return false;
    }

    sockaddr_in bound_addr{};
    socklen_t bound_len = sizeof(bound_addr);
    if (getsockname(fd_, reinterpret_cast<sockaddr*>(&bound_addr), &bound_len) == 0) {
        bound_port_ = ntohs(bound_addr.sin_port);
    } else {
        bound_port_ = bind_port;
    }
    return true;
}

void UdpSocket::Close() {
    if (fd_ >= 0) {
        (void)close(fd_);
    }
    fd_ = -1;
    bound_port_ = 0;
}

bool UdpSocket::IsOpen() const {
    return fd_ >= 0;
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
    if (fd_ < 0) {
        if (error_out != nullptr) {
            *error_out = "socket is closed";
        }
        return false;
    }

    const sockaddr_in addr = ToSockAddr(endpoint);
    const ssize_t sent = sendto(
        fd_,
        bytes,
        size,
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
    if (fd_ < 0) {
        return std::nullopt;
    }

    UdpPacket packet;
    sockaddr_in from{};
    socklen_t from_len = sizeof(from);
    const ssize_t received = recvfrom(
        fd_,
        packet.bytes.data(),
        packet.bytes.size(),
        0,
        reinterpret_cast<sockaddr*>(&from),
        &from_len
    );
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return std::nullopt;
        }
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
    return addresses;
}

} // namespace splonks::network
