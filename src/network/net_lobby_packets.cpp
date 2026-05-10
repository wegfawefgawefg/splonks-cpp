#include "network/net_lobby_internal.hpp"

#include <algorithm>
#include <string>

namespace splonks::network {

void SendEncodedPacket(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const EncodedNetPacket& encoded
) {
    if (transport.capture_outgoing_packets) {
        UdpPacket packet;
        packet.endpoint = endpoint;
        packet.size = std::min(encoded.size, packet.bytes.size());
        std::copy_n(encoded.bytes.begin(), packet.size, packet.bytes.begin());
        transport.captured_packets.push_back(packet);
        return;
    }

    std::string error;
    if (!transport.socket.Send(endpoint, encoded.bytes.data(), encoded.size, &error)) {
        transport.last_error = error;
    }
}

} // namespace splonks::network
