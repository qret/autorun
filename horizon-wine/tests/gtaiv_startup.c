#include <assert.h>
#include <stdio.h>
#include "gtaiv_startup.h"

static unsigned char image[4096];
static const unsigned char code[] = {
    0xe8, 0x09, 0x00, 0x00, 0x00, '1', '9', '1', '1', '.', 'd', 'l', 'l', 0x00,
    0x90, 0xff, 0x15, 0x00, 0x00, 0x00, 0x00, 0x6a, 0x02, 0x50,
    0xff, 0x15, 0x00, 0x00, 0x00, 0x00, 0xff, 0xd0, 0xe9, 0x00, 0x00, 0x00, 0x00
};
static const unsigned char nvapi_loop[] = {
    0x83, 0xf8, 0xf9, 0x74, 0x18, 0x83, 0xf8, 0xfb, 0x74, 0x13, 0x57, 0x56,
    0xe8, 0x00, 0x00, 0x00, 0x00, 0x83, 0xc4, 0x08, 0x46, 0x83, 0xc7, 0x04,
    0x83, 0xf8, 0xfd, 0x75, 0xe3
};

static void prepare_nvapi( size_t offset, size_t wrapper )
{
    static const unsigned char query[] = { 0x68, 0x0d, 0xd4, 0xbd, 0x9a, 0xff, 0x15 };
    int32_t displacement = wrapper - offset - 17;
    memcpy( image + offset, nvapi_loop, sizeof(nvapi_loop) );
    memcpy( image + offset + 13, &displacement, sizeof(displacement) );
    memcpy( image + wrapper + 0x5f, query, sizeof(query) );
}

static void prepare( size_t entry, uint32_t base )
{
    uint32_t import = base + 2048;
    int32_t jump = 32 - (int32_t)(entry + sizeof(code));
    memset( image, 0, sizeof(image) );
    memcpy( image + entry, code, sizeof(code) );
    memcpy( image + entry + 17, &import, sizeof(import) );
    import += 4;
    memcpy( image + entry + 26, &import, sizeof(import) );
    memcpy( image + entry + 33, &jump, sizeof(jump) );
}

int main( void )
{
    size_t call;
    assert( gtaiv_startup_target("ums0:/GTA4/GTAIV.exe", 0x14c) );
    assert( gtaiv_startup_target("D:\\Games\\gtaiv.EXE", 0x14c) );
    assert( !gtaiv_startup_target("PlayGTAIV.exe", 0x14c) );
    assert( !gtaiv_startup_target("GTAIV.exe.bak", 0x14c) );
    assert( !gtaiv_startup_target("GTAIV.exe", 0x8664) );

    for (uint32_t base = 0x400000; base <= 0xd90000; base += 0x330000)
    for (size_t entry = 64; entry <= 256; entry += 64)
    {
        prepare( entry, base );
        call = 0;
        assert( gtaiv_startup_scan(image, sizeof(image), base, entry, &call) && call == entry + 30 );
        for (size_t i = 0; i < 33; i++)
        {
            if ((i >= 17 && i < 21) || (i >= 26 && i < 30)) continue;
            image[entry + i] ^= 1;
            assert( !gtaiv_startup_scan(image, sizeof(image), base, entry, &call) );
            image[entry + i] ^= 1;
        }
        assert( !gtaiv_startup_scan(image, entry + sizeof(code) - 1, base, entry, &call) );
        assert( !gtaiv_startup_scan(image, sizeof(image), base, SIZE_MAX, &call) );
        memcpy( image + call, gtaiv_startup_replacement, sizeof(gtaiv_startup_replacement) );
        assert( !gtaiv_startup_scan(image, sizeof(image), base, entry, &call) );
        assert( !memcmp(image + entry, code, 17) );
        assert( image[entry + 32] == 0xe9 );
    }

    prepare( 64, 0x400000 );
    for (size_t offset = 17; offset <= 26; offset += 9)
    {
        uint32_t invalid[] = { 0x3fffff, 0x401000, 0x400fff, UINT32_MAX };
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
        {
            memcpy( image + 64 + offset, &invalid[i], sizeof(invalid[i]) );
            assert( !gtaiv_startup_scan(image, sizeof(image), 0x400000, 64, &call) );
        }
        prepare( 64, 0x400000 );
    }
    for (int32_t jump = -1024; jump <= 8192; jump += 9216)
    {
        memcpy( image + 64 + 33, &jump, sizeof(jump) );
        assert( !gtaiv_startup_scan(image, sizeof(image), 0x400000, 64, &call) );
    }
    memset( image, 0, sizeof(image) );
    prepare_nvapi( 64, 512 );
    call = 0;
    assert( gtaiv_nvapi_scan(image, sizeof(image), 32, 1024, &call) && call == 88 );
    for (size_t i = 0; i < sizeof(nvapi_loop); i++)
    {
        if (i >= 13 && i < 17) continue;
        image[64 + i] ^= 1;
        call = 0;
        assert( gtaiv_nvapi_scan(image, sizeof(image), 32, 1024, &call) && !call );
        image[64 + i] ^= 1;
    }
    call = 0;
    image[512 + 0x60] ^= 1;
    assert( gtaiv_nvapi_scan(image, sizeof(image), 32, 1024, &call) && !call );
    image[512 + 0x60] ^= 1;
    assert( gtaiv_nvapi_scan(image, sizeof(image), 32, 512, &call) && !call );
    assert( gtaiv_nvapi_scan(image, sizeof(image), 64, sizeof(nvapi_loop) - 1, &call) && !call );
    assert( !gtaiv_nvapi_scan(image, sizeof(image), SIZE_MAX, 32, &call) );
    assert( !gtaiv_nvapi_scan(image, sizeof(image), 32, SIZE_MAX, &call) );
    prepare_nvapi( 256, 512 );
    assert( !gtaiv_nvapi_scan(image, sizeof(image), 32, 1024, &call) );
    call = 0;
    memset( image + 256, 0, sizeof(nvapi_loop) );
    assert( gtaiv_nvapi_scan(image, sizeof(image), 32, 1024, &call) && call == 88 );
    memcpy( image + call, gtaiv_nvapi_replacement, sizeof(gtaiv_nvapi_replacement) );
    call = 0;
    assert( gtaiv_nvapi_scan(image, sizeof(image), 32, 1024, &call) && !call );
    assert( !memcmp(image + 64, nvapi_loop, 13) );
    assert( !memcmp(image + 64 + 17, nvapi_loop + 17, 7) );
    /* TEST/JZ repeats for success (0); every error exits the enumeration. */
    assert( gtaiv_nvapi_replacement[0] == 0x85 && gtaiv_nvapi_replacement[1] == 0xc0 );
    assert( gtaiv_nvapi_replacement[3] == 0x74 && 29 + (int8_t)gtaiv_nvapi_replacement[4] == 0 );
    puts( "GTA IV startup and NVAPI: signatures, call targets, bounds and ambiguous matches passed" );
    return 0;
}
