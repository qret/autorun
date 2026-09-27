#include <assert.h>
#include <stdio.h>
#include "ground_zeroes_startup.h"

static unsigned char image[4096];
static const unsigned char gate[] = {
    0x84, 0xdb, 0x75, 0x7b, 0xff, 0x15, 0, 0, 0, 0, 0x85, 0xc0, 0x74, 0x11,
    0x4d, 0x85, 0xf6, 0x0f, 0x84, 0, 0, 0, 0, 0x49, 0x8b, 0xce, 0xe9
};
static const unsigned char child[] = {
    0xff, 0x15, 0, 0, 0, 0, 0x8b, 0xd8, 0xe8, 0, 0, 0, 0,
    0x3b, 0xc3, 0x75, 0xd5, 0x48, 0x89, 0x7c, 0x24, 0x30, 0x4c, 0x8b, 0xe7
};
static const unsigned char handshake[] = {
    0xc7, 0x05, 0, 0, 0, 0, 0x00, 0x02, 0x00, 0x00, 0xcc, 0xeb, 0x00,
    0x8b, 0x05, 0xf7, 0xff, 0xff, 0xff, 0xf3, 0xc3
};

static void prepare( size_t gate_rva, size_t handshake_rva )
{
    int32_t displacement = (int32_t)handshake_rva - (int32_t)(gate_rva + 0x8c);
    memcpy( image + gate_rva, gate, sizeof(gate) );
    memcpy( image + gate_rva + 0x7f, child, sizeof(child) );
    memcpy( image + gate_rva + 0x88, &displacement, sizeof(displacement) );
    memcpy( image + handshake_rva, handshake, sizeof(handshake) );
}

int main( void )
{
    size_t patch;
    assert( ground_zeroes_startup_target("ums0:/games/MgsGroundZeroes.exe", 0x8664) );
    assert( ground_zeroes_startup_target("D:\\games\\MGSGROUNDZEROES.EXE", 0x8664) );
    assert( !ground_zeroes_startup_target("MgsGroundZeroes.exe", 0x14c) );
    assert( !ground_zeroes_startup_target("MgsGroundZeroes.exe.bak", 0x8664) );
    assert( !ground_zeroes_startup_target("Other.exe", 0x8664) );

    for (size_t gate_rva = 64; gate_rva <= 1024; gate_rva += 64)
    {
        memset( image, 0, sizeof(image) );
        prepare( gate_rva, 2048 );
        patch = 0;
        assert( ground_zeroes_startup_scan(image, sizeof(image), 32, 3000, &patch) && patch == gate_rva );
        assert( !ground_zeroes_startup_scan(image, sizeof(image), 32, 3000, &patch) );
        patch = 0;
        assert( !ground_zeroes_startup_scan(image, sizeof(image), SIZE_MAX, 1, &patch) );
        assert( !ground_zeroes_startup_scan(image, sizeof(image), 32, SIZE_MAX, &patch) );
        assert( ground_zeroes_startup_scan(image, sizeof(image), gate_rva, 0x7f + sizeof(child) - 1, &patch) && !patch );
        assert( ground_zeroes_startup_scan(image, sizeof(image), 32, 2048 + sizeof(handshake) - 33, &patch) && !patch );
        for (size_t j = 0; j < sizeof(handshake); j++)
        {
            image[2048 + j] ^= 1;
            assert( ground_zeroes_startup_scan(image, sizeof(image), 32, 3000, &patch) && !patch );
            image[2048 + j] ^= 1;
        }
        int32_t invalid = INT32_MIN;
        memcpy( image + gate_rva + 0x88, &invalid, sizeof(invalid) );
        assert( ground_zeroes_startup_scan(image, sizeof(image), 32, 3000, &patch) && !patch );
        invalid = INT32_MAX;
        memcpy( image + gate_rva + 0x88, &invalid, sizeof(invalid) );
        assert( ground_zeroes_startup_scan(image, sizeof(image), 32, 3000, &patch) && !patch );
        prepare( gate_rva, 32 );
        assert( ground_zeroes_startup_scan(image, sizeof(image), 32, 3000, &patch) && patch == gate_rva );
        patch = 0;
        prepare( 3072, 2048 );
        assert( !ground_zeroes_startup_scan(image, sizeof(image), 32, sizeof(image) - 32, &patch) );
        memset( image + 3072, 0, 256 );
        patch = 0;
        memcpy( image + gate_rva, ground_zeroes_startup_replacement, sizeof(ground_zeroes_startup_replacement) );
        assert( ground_zeroes_startup_scan(image, sizeof(image), 32, 3000, &patch) && !patch );
        int32_t jump;
        memcpy( &jump, image + gate_rva + 1, sizeof(jump) );
        assert( 5 + jump == 0x7f + 17 );
        assert( !memcmp(image + gate_rva + 0x90, child + 17, sizeof(child) - 17) );
    }
    puts( "Ground Zeroes startup checks passed" );
    return 0;
}
