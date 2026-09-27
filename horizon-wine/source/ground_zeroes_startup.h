#ifndef WINE_NX_GROUND_ZEROES_STARTUP_H
#define WINE_NX_GROUND_ZEROES_STARTUP_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

static const unsigned char ground_zeroes_startup_replacement[] = {
    0xe9, 0x8b, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90
};

static inline int ground_zeroes_startup_target( const char *path, uint16_t machine )
{
    const char *name = path;
    for (const char *p = path; *p; p++) if (*p == '/' || *p == '\\') name = p + 1;
    return machine == 0x8664 && !strcasecmp( name, "MgsGroundZeroes.exe" );
}

static inline int ground_zeroes_startup_scan( const void *image, size_t size,
                                             size_t code_rva, size_t code_size, size_t *patch_rva )
{
    static const signed short gate[] = {
        0x84, 0xdb, 0x75, 0x7b, 0xff, 0x15, -1, -1, -1, -1, 0x85, 0xc0, 0x74, 0x11,
        0x4d, 0x85, 0xf6, 0x0f, 0x84, -1, -1, -1, -1, 0x49, 0x8b, 0xce, 0xe9
    };
    static const signed short child[] = {
        0xff, 0x15, -1, -1, -1, -1, 0x8b, 0xd8, 0xe8, -1, -1, -1, -1,
        0x3b, 0xc3, 0x75, 0xd5, 0x48, 0x89, 0x7c, 0x24, 0x30, 0x4c, 0x8b, 0xe7
    };
    static const signed short handshake[] = {
        0xc7, 0x05, -1, -1, -1, -1, 0x00, 0x02, 0x00, 0x00, 0xcc, 0xeb, 0x00,
        0x8b, 0x05, -1, -1, -1, -1, 0xf3, 0xc3
    };
    const unsigned char *bytes = (const unsigned char *)image;
    const size_t length = 0x7f + sizeof(child) / sizeof(child[0]);

    if (code_rva > size || code_size > size - code_rva) return 0;
    for (size_t i = 0; i + length <= code_size; i++)
    {
        const unsigned char *p = bytes + code_rva + i;
        int32_t displacement, store, load;
        int64_t function;
        size_t j;

        if (*p != gate[0]) continue;
        for (j = 1; j < sizeof(gate) / sizeof(gate[0]); j++)
            if (gate[j] >= 0 && p[j] != gate[j]) break;
        if (j != sizeof(gate) / sizeof(gate[0])) continue;
        for (j = 0; j < sizeof(child) / sizeof(child[0]); j++)
            if (child[j] >= 0 && p[0x7f + j] != child[j]) break;
        if (j != sizeof(child) / sizeof(child[0])) continue;
        memcpy( &displacement, p + 0x88, sizeof(displacement) );
        function = (int64_t)(code_rva + i + 0x8c) + displacement;
        if (function < (int64_t)code_rva || (uint64_t)function > code_rva + code_size ||
            sizeof(handshake) / sizeof(handshake[0]) > code_rva + code_size - function) continue;
        for (j = 0; j < sizeof(handshake) / sizeof(handshake[0]); j++)
            if (handshake[j] >= 0 && bytes[function + j] != handshake[j]) break;
        if (j != sizeof(handshake) / sizeof(handshake[0])) continue;
        memcpy( &store, bytes + function + 2, sizeof(store) );
        memcpy( &load, bytes + function + 15, sizeof(load) );
        if ((int64_t)store + 10 != (int64_t)load + 19) continue;
        if (*patch_rva) return 0;
        /* Enter the game after the debugger's child-PID handshake. */
        *patch_rva = code_rva + i;
    }
    return 1;
}

#endif
