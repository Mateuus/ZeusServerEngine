#pragma once

#include <string_view>

namespace Zeus::Platform
{
/**
 * Atualiza o título da janela de consola (Windows: barra de título / separador;
 * outros terminais: sequência OSC quando suportado).
 * UTF-8; texto curto recomendado (ex. TPS em tempo real).
 */
void SetConsoleTitleUtf8(std::string_view titleUtf8);
} // namespace Zeus::Platform
