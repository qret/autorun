#include <assert.h>
#include <stdio.h>
#include "gears_startup.h"

static unsigned char image[4096];
static const unsigned char code[] = {
    0x8b, 0xc8, 0xe8, 0x91, 0xfe, 0xff, 0xff, 0x84, 0xc0, 0x75, 0x5d,
    0x6a, 0x00, 0x6a, 0x00, 0x68, 0xd8, 0xb6, 0x95, 0x01, 0x68, 0xf0, 0xb6, 0x95, 0x01,
    0x8d, 0x44, 0x24, 0x14, 0x68, 0x20, 0xb7, 0x95, 0x01, 0x50, 0xe8, 0x70, 0x55, 0xe4, 0xff,
    0x83, 0xc4, 0x18
};

static void prepare( size_t offset, uint32_t base, size_t key_rva )
{
    static const char key[] = "RunStartupError_Message";
    uint32_t address = base + key_rva;

    memcpy( image + offset, code, sizeof(code) );
    memcpy( image + offset + 21, &address, sizeof(address) );
    for (size_t i = 0; i < sizeof(key); i++)
    {
        image[key_rva + i * 2] = key[i];
        image[key_rva + i * 2 + 1] = 0;
    }
}

int main( void )
{
    size_t call;
    assert( gears_startup_target("ums0:/Gears of War/Binaries/WarGame-G4WLive.exe", 0x14c) );
    assert( gears_startup_target("D:\\Games\\WARGAME-G4WLIVE.EXE", 0x14c) );
    assert( !gears_startup_target("Startup.exe", 0x14c) );
    assert( !gears_startup_target("NotWarGame-G4WLive.exe", 0x14c) );
    assert( !gears_startup_target("WarGame-G4WLive.exe.bak", 0x14c) );
    assert( !gears_startup_target("WarGame-G4WLive.exe", 0x8664) );

    for (uint32_t base = 0x400000; base <= 0x800000; base += 0x400000)
    for (size_t offset = 64; offset <= 256; offset += 64)
    {
        memset( image, 0, sizeof(image) );
        prepare( offset, base, 2048 );
        call = 0;
        assert( gears_startup_scan(image, sizeof(image), base, 32, 1024, &call) && call == offset + 2 );
        for (size_t i = 0; i < sizeof(code); i++)
        {
            if ((i >= 3 && i <= 6) || i == 10 || (i >= 16 && i <= 19) ||
                (i >= 30 && i <= 33) || (i >= 36 && i <= 39)) continue;
            image[offset + i] ^= 1;
            call = 0;
            assert( gears_startup_scan(image, sizeof(image), base, 32, 1024, &call) && !call );
            image[offset + i] ^= 1;
        }
        image[offset + 3] ^= 0x7f;
        image[offset + 16] ^= 0x7f;
        call = 0;
        assert( gears_startup_scan(image, sizeof(image), base, offset, sizeof(code), &call) && call == offset + 2 );
        call = 0;
        assert( gears_startup_scan(image, sizeof(image), base, offset, sizeof(code) - 1, &call) && !call );
        image[2048 + 6] ^= 1;
        assert( gears_startup_scan(image, sizeof(image), base, 32, 1024, &call) && !call );
        image[2048 + 6] ^= 1;
        assert( gears_startup_scan(image, 2050, base, 32, 1024, &call) && !call );
        assert( !gears_startup_scan(image, sizeof(image), base, SIZE_MAX, 1024, &call) );
        assert( !gears_startup_scan(image, sizeof(image), base, 32, SIZE_MAX, &call) );
        memcpy( image + offset + 2, gears_startup_replacement, sizeof(gears_startup_replacement) );
        assert( gears_startup_scan(image, sizeof(image), base, 32, 1024, &call) && !call );
        assert( !memcmp(image + offset + 7, code + 7, 8) );
    }
    memset( image, 0, sizeof(image) );
    prepare( 64, 0x400000, 2048 );
    prepare( 256, 0x400000, 2048 );
    call = 0;
    assert( !gears_startup_scan(image, sizeof(image), 0x400000, 32, 1024, &call) );
    call = 0;
    assert( gears_startup_scan(image, sizeof(image), 0x400000, 64, sizeof(code), &call) && call == 66 );
    assert( !gears_startup_scan(image, sizeof(image), 0x400000, 256, sizeof(code), &call) );
    puts( "Gears startup: paths, relocation, instruction checks, bounds and ambiguous matches passed" );
    return 0;
}
