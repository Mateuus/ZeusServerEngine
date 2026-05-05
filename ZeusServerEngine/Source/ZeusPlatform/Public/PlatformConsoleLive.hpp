#pragma once

#include "ZeusResult.hpp"

#include <cstdint>
#include <string_view>

namespace Zeus::Platform
{
/**
 * Painel de debug no fundo do terminal (stderr): N linhas atualizáveis in-place (VT + DECSTBM).
 * Requer TTY; com stderr redirecionado, Init devolve Ok e operações são no-op.
 * Usar sempre sob o mesmo mutex que ZeusLog (serialização automática ao chamar APIs públicas).
 */
struct ConsoleLivePanel
{
    static ZeusResult Init(std::uint32_t reservedRows);
    static void Shutdown();
    static void SetSlot(std::uint32_t index, std::string_view lineUtf8);
    static void ClearSlot(std::uint32_t index);
    /** Linha acima da zona de slots (só se existir espaço). */
    static void SetHeader(std::string_view lineUtf8);
};
} // namespace Zeus::Platform
