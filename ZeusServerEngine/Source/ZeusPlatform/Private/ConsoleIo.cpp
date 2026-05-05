#include "ConsoleIo.hpp"

namespace Zeus::Platform::Detail
{
std::mutex& ConsoleIoMutex()
{
    static std::mutex instance;
    return instance;
}
} // namespace Zeus::Platform::Detail
