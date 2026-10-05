#ifndef AUTORUN_FF7REMAKE_H
#define AUTORUN_FF7REMAKE_H

#include <stdint.h>
#include <string.h>
#include <strings.h>

static inline int ff7remake_target( const char *target, uint16_t machine )
{
    const char *name = target;
    for (const char *p = target; *p; p++) if (*p == '/' || *p == '\\') name = p + 1;
    return machine == 0x8664 && !strcasecmp( name, "ff7remake_.exe" );
}

static inline int ff7remake_config_scan( const void *image, size_t size, size_t code_rva,
                                        size_t code_size, size_t *branch_rva )
{
    static const signed short pattern[] = {
        0x49, 0x8d, 0x96, 0xa0, 0, 0, 0, 0x41, 0xb8, 1, 0, 0, 0, 0x48, 0x8b, 0xce,
        0x83, 0x38, 0xff, 0x40, 0x0f, 0x94, 0xc7, 0xe8, -1, -1, -1, -1,
        0x33, 0xdb, 0x84, 0xc0, 0x74, 0x1c, 0x49, 0x8d, 0x96, 0xb0, 0, 0, 0,
        0x48, 0x8b, 0xce, 0x44, 0x8d, 0x43, 1, 0xe8, -1, -1, -1, -1,
        0x84, 0xc0, 0x40, 0x0f, 0xb6, 0xff, 0x0f, 0x44, 0xfb, 0x65, 0x48, 0x8b, 4, 0x25, 0x58, 0, 0, 0
    };
    const unsigned char *bytes = image;
    const size_t length = sizeof(pattern) / sizeof(pattern[0]);

    if (code_rva > size || code_size > size - code_rva) return 0;
    for (size_t i = 0; i + length <= code_size; i++)
    {
        const unsigned char *p = bytes + code_rva + i;
        int32_t first, second;
        size_t j;

        if (*p != pattern[0]) continue;
        for (j = 1; j < length; j++) if (pattern[j] >= 0 && p[j] != pattern[j]) break;
        if (j != length) continue;
        memcpy( &first, p + 24, sizeof(first) );
        memcpy( &second, p + 49, sizeof(second) );
        if ((int64_t)first - second != 25 || *branch_rva) return 0;
        /* Keep the asset blacklist; remove only the GameUserSettings.ini restriction. */
        *branch_rva = code_rva + i + 32;
    }
    return 1;
}

#endif
