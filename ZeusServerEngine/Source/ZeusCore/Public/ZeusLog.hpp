#pragma once

#include "ZeusResult.hpp"

#include <filesystem>
#include <string_view>

namespace Zeus
{
enum class LogLevel : int
{
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4
};

/**
 * Thread-safe logging para stderr + ficheiro de sessão opcional.
 * Implementação em ZeusPlatform (mutex partilhado com título e painel VT).
 */
class ZeusLog
{
public:
    static void SetMinimumLevel(LogLevel level);
    static LogLevel GetMinimumLevel();

    static void Write(LogLevel level, std::string_view category, std::string_view message);

    static void Trace(std::string_view category, std::string_view message);
    static void Debug(std::string_view category, std::string_view message);
    static void Info(std::string_view category, std::string_view message);
    static void Warning(std::string_view category, std::string_view message);
    static void Error(std::string_view category, std::string_view message);

    /**
     * Cria `logDirectory/ZeusServer_YYYYMMDD_HHMMSS.log` e duplica todas as linhas de log.
     * Chamado uma vez no arranque; falha silenciosa em stderr redirecionado se não for possível criar ficheiros.
     */
    static ZeusResult OpenSessionLog(const std::filesystem::path& logDirectory);

    /** Fecha o ficheiro de sessão (chamar no shutdown após o último log). */
    static void CloseSessionLog();

    [[nodiscard]] static bool IsSessionLogOpen();

    /** Caminho do `.log` da sessão atual; vazio se não houver ficheiro aberto. */
    [[nodiscard]] static std::filesystem::path GetSessionLogPath();

private:
    static const char* LevelName(LogLevel level);
};
} // namespace Zeus
