#pragma once

#include <mutex>

namespace Zeus::Platform::Detail
{
/** Mutex único para stderr + título + painel VT (evita cursores partidos). */
std::mutex& ConsoleIoMutex();
} // namespace Zeus::Platform::Detail
