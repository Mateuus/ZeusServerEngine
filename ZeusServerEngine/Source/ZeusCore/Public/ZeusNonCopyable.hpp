#pragma once

namespace Zeus
{
/** Base for types that must not be copied (loops, OS handles wrappers). */
class NonCopyable
{
public:
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;
    virtual ~NonCopyable() = default;
};
} // namespace Zeus
