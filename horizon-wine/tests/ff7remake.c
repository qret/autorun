#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "games/ff7remake.h"
#include "launcher_pe.h"

static const unsigned char gate[] = {
    0x49, 0x8d, 0x96, 0xa0, 0, 0, 0, 0x41, 0xb8, 1, 0, 0, 0, 0x48, 0x8b, 0xce,
    0x83, 0x38, 0xff, 0x40, 0x0f, 0x94, 0xc7, 0xe8, 0x48, 0x71, 0x3d, 0xfe,
    0x33, 0xdb, 0x84, 0xc0, 0x74, 0x1c, 0x49, 0x8d, 0x96, 0xb0, 0, 0, 0,
    0x48, 0x8b, 0xce, 0x44, 0x8d, 0x43, 1, 0xe8, 0x2f, 0x71, 0x3d, 0xfe,
    0x84, 0xc0, 0x40, 0x0f, 0xb6, 0xff, 0x0f, 0x44, 0xfb, 0x65, 0x48, 0x8b, 4, 0x25, 0x58, 0, 0, 0
};

int main( int argc, char **argv )
{
    unsigned char image[512] = {0};
    size_t branch = 0;

    assert( ff7remake_target( "ums0:/Games/FF7REMAKE_.EXE", 0x8664 ) );
    assert( !ff7remake_target( "ff7remake.exe", 0x8664 ) );
    assert( !ff7remake_target( "ff7remake_.exe", 0x014c ) );
    memcpy( image + 32, gate, sizeof(gate) );
    assert( ff7remake_config_scan( image, sizeof(image), 16, 160, &branch ) && branch == 64 );
    for (size_t i = 0; i < sizeof(gate); i++)
    {
        image[32 + i] ^= 1;
        branch = 0;
        int valid = ff7remake_config_scan( image, sizeof(image), 16, 160, &branch );
        assert( !branch && valid == !((i >= 24 && i < 28) || (i >= 49 && i < 53)) );
        image[32 + i] ^= 1;
    }
    assert( !ff7remake_config_scan( image, sizeof(image), SIZE_MAX, 160, &branch ) );
    assert( !ff7remake_config_scan( image, sizeof(image), 16, SIZE_MAX, &branch ) );
    assert( ff7remake_config_scan( image, sizeof(image), 32, sizeof(gate) - 1, &branch ) && !branch );
    memcpy( image + 192, gate, sizeof(gate) );
    assert( !ff7remake_config_scan( image, sizeof(image), 16, 320, &branch ) );

    if (argc == 2)
    {
        struct launcher_pe pe;
        unsigned char *mapped;
        size_t size = 0;

        assert( launcher_pe_open( &pe, argv[1] ) );
        for (unsigned int i = 0; i < pe.section_count; i++)
        {
            const struct launcher_pe_section *section = pe.sections + i;
            if ((size_t)section->va + section->raw_size > size) size = (size_t)section->va + section->raw_size;
        }
        mapped = calloc( 1, size );
        assert( mapped );
        branch = 0;
        for (unsigned int i = 0; i < pe.section_count; i++)
        {
            const struct launcher_pe_section *section = pe.sections + i;
            assert( launcher_pe_read_at( &pe, section->raw_offset, mapped + section->va, section->raw_size ) );
            assert( ff7remake_config_scan( mapped, size, section->va, section->raw_size, &branch ) );
        }
        assert( branch );
        printf( "Verified executable INI gate at RVA %#zx\n", branch );
        free( mapped );
        launcher_pe_close( &pe );
    }
    puts( "FF7 configuration gate: target, signature, calls, bounds and uniqueness passed" );
    return 0;
}
