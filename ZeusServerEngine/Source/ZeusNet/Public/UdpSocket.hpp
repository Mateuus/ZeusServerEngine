#pragma once

#include "UdpEndpoint.hpp"
#include "ZeusResult.hpp"

#include <cstddef>
#include <cstdint>

namespace Zeus::Net
{
/** Socket UDP não bloqueante (Windows: WinSock2; Linux: BSD sockets). */
class UdpSocket
{
public:
    UdpSocket();
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    [[nodiscard]] ZeusResult Open();
    void Close();
    [[nodiscard]] bool IsOpen() const;

    [[nodiscard]] ZeusResult Bind(std::uint16_t portHostOrder);

    /**
     * Tenta receber um datagrama.
     * @return Ok com `outSize == 0` se não houver pacote (non-blocking).
     */
    [[nodiscard]] ZeusResult TryRecvFrom(std::uint8_t* buffer, std::size_t bufferSize, std::size_t& outSize, UdpEndpoint& outFrom);

    [[nodiscard]] ZeusResult SendTo(const UdpEndpoint& to, const std::uint8_t* data, std::size_t size);

private:
    /** Windows: valor `SOCKET` como inteiro; inválido = `INVALID_SOCKET`. Linux: fd >= 0. */
    std::intptr_t nativeSocket_ = -1;
};
} // namespace Zeus::Net
