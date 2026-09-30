#ifndef WINE_NX_GTAIV_STARTUP_H
#define WINE_NX_GTAIV_STARTUP_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

static const unsigned char gtaiv_startup_replacement[] = { 0x90, 0x90 };
static const unsigned char gtaiv_nvapi_replacement[] = { 0x85, 0xc0, 0x90, 0x74, 0xe3 };

static inline int gtaiv_startup_target( const char *path, uint16_t machine )
{
    const char *name = path;
    for (const char *p = path; *p; p++) if (*p == '/' || *p == '\\') name = p + 1;
    return machine == 0x014c && !strcasecmp( name, "GTAIV.exe" );
}

static inline int gtaiv_startup_scan( const void *image, size_t size, uintptr_t base,
                                     size_t entry_rva, size_t *call_rva )
{
    static const signed short pattern[] = {
        0xe8, 0x09, 0x00, 0x00, 0x00, '1', '9', '1', '1', '.', 'd', 'l', 'l', 0x00,
        0x90, 0xff, 0x15, -1, -1, -1, -1, 0x6a, 0x02, 0x50,
        0xff, 0x15, -1, -1, -1, -1, 0xff, 0xd0, 0xe9, -1, -1, -1, -1
    };
    const size_t length = sizeof(pattern) / sizeof(pattern[0]);
    const unsigned char *p;
    uint32_t import;
    int32_t displacement;
    int64_t destination;

    if (entry_rva > size || length > size - entry_rva) return 0;
    p = (const unsigned char *)image + entry_rva;
    for (size_t i = 0; i < length; i++) if (pattern[i] >= 0 && p[i] != pattern[i]) return 0;
    for (size_t i = 17; i <= 26; i += 9)
    {
        memcpy( &import, p + i, sizeof(import) );
        if (import < base || import - base > size || sizeof(import) > size - (import - base)) return 0;
    }
    memcpy( &displacement, p + 33, sizeof(displacement) );
    destination = (int64_t)entry_rva + length + displacement;
    if (destination < 0 || (uint64_t)destination >= size) return 0;
    /* Keep DLL loading intact; skip only its DirectComposition intro. */
    *call_rva = entry_rva + 30;
    return 1;
}

static inline int gtaiv_nvapi_scan( const void *image, size_t size,
                                   size_t code_rva, size_t code_size, size_t *patch_rva )
{
    static const signed short pattern[] = {
        0x83, 0xf8, 0xf9, 0x74, 0x18, 0x83, 0xf8, 0xfb, 0x74, 0x13, 0x57, 0x56,
        0xe8, -1, -1, -1, -1, 0x83, 0xc4, 0x08, 0x46, 0x83, 0xc7, 0x04,
        0x83, 0xf8, 0xfd, 0x75, 0xe3
    };
    static const unsigned char query[] = { 0x68, 0x0d, 0xd4, 0xbd, 0x9a, 0xff, 0x15 };
    const unsigned char *bytes = (const unsigned char *)image;
    const size_t length = sizeof(pattern) / sizeof(pattern[0]);

    if (code_rva > size || code_size > size - code_rva) return 0;
    for (size_t i = 0; i + length <= code_size; i++)
    {
        const unsigned char *p = bytes + code_rva + i;
        int32_t displacement;
        int64_t function;
        size_t j;

        if (*p != pattern[0]) continue;
        for (j = 1; j < length; j++) if (pattern[j] >= 0 && p[j] != pattern[j]) break;
        if (j != length) continue;
        memcpy( &displacement, p + 13, sizeof(displacement) );
        function = (int64_t)(code_rva + i + 17) + displacement;
        if (function < (int64_t)code_rva || (uint64_t)function > code_rva + code_size ||
            0x5f + sizeof(query) > code_rva + code_size - function) continue;
        if (memcmp( bytes + function + 0x5f, query, sizeof(query) )) continue;
        if (*patch_rva) return 0;
        /* Stop enumeration on any NvAPI error. */
        *patch_rva = code_rva + i + 24;
    }
    return 1;
}

#endif
