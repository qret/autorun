#ifndef AUTORUN_FALLOUT_STARTUP_H
#define AUTORUN_FALLOUT_STARTUP_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

static const unsigned char fallout_startup_replacement[] = { 0x30, 0xc0, 0x90, 0x90, 0x90 };

static inline const char *fallout_startup_launcher( const char *path, uint16_t machine )
{
    const char *name = path;
    for (const char *p = path; *p; p++) if (*p == '/' || *p == '\\') name = p + 1;
    if (machine != 0x014c) return NULL;
    if (!strcasecmp( name, "Fallout3.exe" )) return "FalloutLauncher.exe";
    if (!strcasecmp( name, "FalloutNV.exe" )) return "FalloutNVLauncher.exe";
    return NULL;
}

static inline int fallout_startup_scan( const void *image, size_t size, uintptr_t base, const char *launcher,
                                       size_t code_rva, size_t code_size, size_t *call_rva )
{
    const unsigned char *bytes = image;
    const size_t launcher_size = strlen(launcher) + 1;

    if (code_rva > size || code_size > size - code_rva) return 0;
    for (size_t i = 0; i + 5 <= code_size; i++)
    {
        uint32_t address;
        if (bytes[code_rva + i] != 0x68) continue;
        memcpy( &address, bytes + code_rva + i + 1, sizeof(address) );
        if (address < base || address - base > size || launcher_size > size - (address - base) ||
            memcmp( bytes + address - base, launcher, launcher_size )) continue;

        for (size_t j = i > 192 ? i - 192 : 0; j + 15 < i; j++)
        {
            const unsigned char *p = bytes + code_rva + j;
            int32_t displacement;
            size_t branch, next;

            if (p[0] != 0xe8) continue;
            if (p[5] == 0x84 && p[6] == 0xc0) branch = 7;
            else if (!memcmp( p + 5, "\x0f\xb6\xc8\x85\xc9", 5 )) branch = 10;
            else continue;
            if (j + branch + 9 > i) continue;
            if (p[branch] == 0x74) { displacement = (int8_t)p[branch + 1]; next = j + branch + 2; }
            else if (p[branch] == 0x0f && p[branch + 1] == 0x84)
            { memcpy( &displacement, p + branch + 2, sizeof(displacement) ); next = j + branch + 6; }
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
