#pragma once

#include <cstdint>
#include <cstdlib>

namespace WineNX::FEX {

// The alias window of a fixed-low 32-bit game (low_window.c): the runtime passes
// log2 of its base, 32 or more, in WINE_NX_FEX_WINDOW_SHIFT; 0 means no window.
inline uint8_t AliasWindowShift() {
  const char* value = std::getenv("WINE_NX_FEX_WINDOW_SHIFT");
  if (!value) {
    return 0;
  }
  char* end;
  const unsigned long shift = std::strtoul(value, &end, 10);
  return *end == '\0' && shift >= 32 && shift <= 46 ? static_cast<uint8_t>(shift) : 0;
}

inline uint64_t AliasWindowBase() {
  static const uint64_t Base = AliasWindowShift() ? uint64_t(1) << AliasWindowShift() : 0;
  return Base;
}

// Where FEX itself finds a 32-bit guest's address, its code included: below
// 128 MiB that is the window, elsewhere the address as it is.
inline uint64_t AliasWindowHost(uint64_t Address) {
  return Address < 0x8000000 ? Address + AliasWindowBase() : Address;
}

}
