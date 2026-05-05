#pragma once

#include <filesystem>

namespace Zeus::App
{
/**
 * Descobre a raiz do servidor (pasta que contém `Config/` e normalmente `Data/`).
 * Ordem: variável de ambiente `ZEUS_SERVER_ROOT`; depois sobe a partir do executável
 * até encontrar `Config/server.json` e, de preferência, uma pasta `Data/`.
 */
std::filesystem::path ResolveContentRoot(const char* argv0);

/** Se `p` for relativo, junta a `contentRoot`; caso contrário devolve `p`. */
std::filesystem::path ResolveUnderContentRoot(const std::filesystem::path& contentRoot,
    const std::filesystem::path& p);
} // namespace Zeus::App
