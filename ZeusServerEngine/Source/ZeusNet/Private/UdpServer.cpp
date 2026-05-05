#include "UdpServer.hpp"

#include "PacketConstants.hpp"

#include <array>

namespace Zeus::Net
{
ZeusResult UdpServer::Start(const std::uint16_t portHostOrder)
{
    Stop();
    ZeusResult o = socket_.Open();
    if (!o.Ok())
    {
        return o;
    }
    ZeusResult b = socket_.Bind(portHostOrder);
    if (!b.Ok())
    {
        Stop();
        return b;
    }
    return ZeusResult::Success();
}

void UdpServer::Stop()
{
    socket_.Close();
}

void UdpServer::PumpReceive(const std::function<void(const std::uint8_t*, std::size_t, const UdpEndpoint&)>& handler)
{
    if (!handler || !socket_.IsOpen())
    {
        return;
    }
    std::array<std::uint8_t, Zeus::Protocol::ZEUS_MAX_PACKET_BYTES> buf{};
    for (;;)
    {
        std::size_t n = 0;
        UdpEndpoint from{};
        const ZeusResult r = socket_.TryRecvFrom(buf.data(), buf.size(), n, from);
        if (!r.Ok() || n == 0)
        {
            break;
        }
        handler(buf.data(), n, from);
    }
}

ZeusResult UdpServer::SendTo(const UdpEndpoint& to, const std::uint8_t* data, const std::size_t size)
{
    return socket_.SendTo(to, data, size);
}
} // namespace Zeus::Net
