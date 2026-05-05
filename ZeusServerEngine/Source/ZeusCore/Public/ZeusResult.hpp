#pragma once

#include <string>
#include <utility>

namespace Zeus
{
enum class ZeusErrorCode : int
{
    Ok = 0,
    InvalidArgument = 1,
    IoError = 2,
    ParseError = 3,
    Unknown = 99
};

/** Lightweight result without exceptions for core/bootstrap paths. */
class ZeusResult
{
public:
    static ZeusResult Success() { return ZeusResult(ZeusErrorCode::Ok, {}); }

    static ZeusResult Failure(ZeusErrorCode code, std::string message)
    {
        return ZeusResult(code, std::move(message));
    }

    [[nodiscard]] bool Ok() const { return code_ == ZeusErrorCode::Ok; }
    [[nodiscard]] ZeusErrorCode GetCode() const { return code_; }
    /** Texto humano (evita nome `GetMessage` — macro no Windows). */
    [[nodiscard]] const std::string& GetErrorText() const { return message_; }

private:
    ZeusResult(ZeusErrorCode code, std::string message)
        : code_(code)
        , message_(std::move(message))
    {
    }

    ZeusErrorCode code_;
    std::string message_;
};
} // namespace Zeus
