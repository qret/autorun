#ifndef AUTORUN_FALLOUT3_STARTUP_H
#define AUTORUN_FALLOUT3_STARTUP_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

static const unsigned char fallout3_startup_replacement[] = { 0x30, 0xc0, 0x90, 0x90, 0x90 };

static inline int fallout3_startup_target( const char *path, uint16_t machine )
{
    const char *name = path;
    for (const char *p = path; *p; p++) if (*p == '/' || *p == '\\') name = p + 1;
    return machine == 0x014c && !strcasecmp( name, "Fallout3.exe" );
}

static inline int fallout3_startup_scan( const void *image, size_t size, uintptr_t base,
                                        size_t code_rva, size_t code_size, size_t *call_rva )
{
    static const char launcher[] = "FalloutLauncher.exe";
    const unsigned char *bytes = image;

    if (code_rva > size || code_size > size - code_rva) return 0;
    for (size_t i = 0; i + 5 <= code_size; i++)
    {
        uint32_t address;
        if (bytes[code_rva + i] != 0x68) continue;
        memcpy( &address, bytes + code_rva + i + 1, sizeof(address) );
        if (address < base || address - base > size || sizeof(launcher) > size - (address - base) ||
            memcmp( bytes + address - base, launcher, sizeof(launcher) )) continue;

        for (size_t j = i > 192 ? i - 192 : 0; j + 15 < i; j++)
        {
            const unsigned char *p = bytes + code_rva + j;
            int32_t displacement;
            size_t next;

            if (p[0] != 0xe8 || p[5] != 0x84 || p[6] != 0xc0) continue;
            if (p[7] == 0x74) { displacement = (int8_t)p[8]; next = j + 9; }
            else if (p[7] == 0x0f && p[8] == 0x84)
            { memcpy( &displacement, p + 9, sizeof(displacement) ); next = j + 13; }
            else continue;
            if (displacement <= 0 || (size_t)displacement > code_size - next || next + displacement <= i + 5 ||
                next + displacement - i > 128 || memcmp( bytes + code_rva + next, "\x6a\x01\xe8", 3 )) continue;
            if (*call_rva) return 0;
            /* Autorun supplies the preferences; do not restart in the hardware-detection launcher. */
            *call_rva = code_rva + j;
        }
    }
    return 1;
}

#endif
