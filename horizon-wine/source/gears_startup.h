#ifndef WINE_NX_GEARS_STARTUP_H
#define WINE_NX_GEARS_STARTUP_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

#define GEARS_STARTUP_ARGS "-seekfreeloading -final_release -installed"

static const unsigned char gears_startup_replacement[] = { 0xb0, 0x01, 0x90, 0x90, 0x90 };

static inline int gears_startup_target( const char *path, uint16_t machine )
{
    const char *name = path;

    for (const char *p = path; *p; p++) if (*p == '/' || *p == '\\') name = p + 1;
    return machine == 0x014c && !strcasecmp( name, "WarGame-G4WLive.exe" );
}

static inline int gears_startup_scan( const void *image, size_t size, uintptr_t base,
                                     size_t code_rva, size_t code_size, size_t *call_rva )
{
    static const char key[] = "RunStartupError_Message";
    static const signed short pattern[] = {
        0x8b, 0xc8, 0xe8, -1, -1, -1, -1, 0x84, 0xc0, 0x75, -1,
        0x6a, 0x00, 0x6a, 0x00, 0x68, -1, -1, -1, -1, 0x68, -1, -1, -1, -1,
        0x8d, 0x44, 0x24, 0x14, 0x68, -1, -1, -1, -1, 0x50, 0xe8, -1, -1, -1, -1,
        0x83, 0xc4, 0x18
    };
    const unsigned char *bytes = image;
    const size_t length = sizeof(pattern) / sizeof(pattern[0]);

    if (code_rva > size || code_size > size - code_rva) return 0;
    for (size_t i = 0; i + length <= code_size; i++)
    {
        const unsigned char *p = bytes + code_rva + i;
        uint32_t address;
        size_t j, key_rva;

        if (*p != pattern[0]) continue;
        for (j = 1; j < length; j++) if (pattern[j] >= 0 && p[j] != pattern[j]) break;
        if (j != length) continue;
        memcpy( &address, p + 21, sizeof(address) );
        if (address < base || address - base > size) continue;
        key_rva = address - base;
        if (sizeof(key) * 2 > size - key_rva) continue;
        for (j = 0; j < sizeof(key); j++)
            if (bytes[key_rva + j * 2] != key[j] || bytes[key_rva + j * 2 + 1]) break;
        if (j != sizeof(key)) continue;
        if (*call_rva) return 0;
        /* Replace the Ripple handshake call, leaving the error and exit paths intact. */
        *call_rva = code_rva + i + 2;
    }
    return 1;
}

#endif
