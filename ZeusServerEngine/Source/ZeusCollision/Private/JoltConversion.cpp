#include "JoltConversion.hpp"

// All conversion helpers are inline templates in the header; this TU exists to
// keep `JoltConversion` linkable as a unit even when not in use.
namespace Zeus::Collision
{
namespace JoltConversion_TUKeepAlive
{
volatile int Sentinel = 0;
}
} // namespace Zeus::Collision
