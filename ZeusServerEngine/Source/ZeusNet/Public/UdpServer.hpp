#pragma once

#include "UdpEndpoint.hpp"
#include "UdpSocket.hpp"
#include "ZeusResult.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace Zeus::Net
{
/** Servidor UDP mínimo: bind + drenagem de `recvfrom` por tick. */
class UdpServer
{
public:
    [[nodiscard]] ZeusResult Start(std::uint16_t portHostOrder);
    void Stop();

    [[nodiscard]] bool IsRunning() const { return socket_.IsOpen(); }

    /** Processa todos os datagramas pendentes (non-blocking). */
    void PumpReceive(const std::function<void(const std::uint8_t* data, std::size_t size, const UdpEndpoint& from)>& handler);

    [[nodiscard]] ZeusResult SendTo(const UdpEndpoint& to, const std::uint8_t* data, std::size_t size);

private:
    UdpSocket socket_;
};
} // namespace Zeus::Net
