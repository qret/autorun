#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../source/fallout_startup.h"

static void put32( unsigned char *p, uint32_t value ) { memcpy( p, &value, sizeof(value) ); }
static uint16_t get16( const unsigned char *p ) { uint16_t v; memcpy( &v, p, 2 ); return v; }
static uint32_t get32( const unsigned char *p ) { uint32_t v; memcpy( &v, p, 4 ); return v; }

static void check_image( const char *path, const char *launcher )
{
    FILE *file = fopen( path, "rb" );
    assert(file);
    assert(!fseek(file, 0, SEEK_END));
    long length = ftell(file);
    assert(length > 64 && !fseek(file, 0, SEEK_SET));
    unsigned char *data = malloc(length);
    assert(data && fread(data, 1, length, file) == (size_t)length && !fclose(file));
    size_t pe = get32(data + 60);
    assert(pe + 248 <= (size_t)length && get32(data + pe) == 0x4550);
    size_t size = get32(data + pe + 80), sections = pe + 24 + get16(data + pe + 20);
    unsigned char *image = calloc(1, size);
    assert(image);
    for (unsigned i = 0; i < get16(data + pe + 6); i++)
    {
        const unsigned char *s = data + sections + 40 * i;
        assert(sections + 40 * (i + 1) <= (size_t)length);
        assert(get32(s + 12) + (uint64_t)get32(s + 16) <= size);
        assert(get32(s + 20) + (uint64_t)get32(s + 16) <= (size_t)length);
        memcpy(image + get32(s + 12), data + get32(s + 20), get32(s + 16));
    }
    assert(launcher && get16(data + pe + 4) == 0x14c);
    size_t call = 0;
    for (unsigned i = 0; i < get16(data + pe + 6); i++)
    {
        const unsigned char *s = data + sections + 40 * i;
        if (get32(s + 36) & 0x20000000)
            assert(fallout_startup_scan(image, size, get32(data + pe + 52), launcher, get32(s + 12), get32(s + 8), &call));
    }
    assert(call);
    printf("%s: launcher check at RVA %zx\n", path, call);
    free(image);
    free(data);
}

int main( int argc, char **argv )
{
    unsigned char image[1024] = {0};
    size_t call = 0;
    assert(fallout_startup_launcher("ums0:/Games/FALLOUT3.EXE", 0x14c));
    assert(!fallout_startup_launcher("FalloutLauncher.exe", 0x14c));
    assert(!fallout_startup_launcher("Fallout3.exe", 0x8664));
    assert(!strcmp(fallout_startup_launcher("D:\\Games\\FALLOUTNV.EXE", 0x14c), "FalloutNVLauncher.exe"));
    assert(!fallout_startup_launcher("FalloutNVLauncher.exe", 0x14c));
    assert(!fallout_startup_launcher("FalloutNV.exe.bak", 0x14c));
    assert(!fallout_startup_launcher("FalloutNV.exe", 0x8664));
    memcpy(image + 800, "FalloutLauncher.exe", 19);
    memcpy(image + 64, "\xe8\x00\x00\x00\x00\x84\xc0\x74\x57\x6a\x01\xe8", 12);
    image[112] = 0x68;
    put32(image + 113, 0x400000 + 800);
    assert(fallout_startup_scan(image, sizeof(image), 0x400000, "FalloutLauncher.exe", 32, 400, &call) && call == 64);
    memcpy(image + 64, fallout_startup_replacement, sizeof(fallout_startup_replacement));
    call = 0;
    assert(fallout_startup_scan(image, sizeof(image), 0x400000, "FalloutLauncher.exe", 32, 400, &call) && !call);
    memcpy(image + 64, "\xe8\x00\x00\x00\x00\x84\xc0\x0f\x84", 9);
    put32(image + 73, 83);
    memcpy(image + 77, "\x6a\x01\xe8", 3);
    assert(fallout_startup_scan(image, sizeof(image), 0x400000, "FalloutLauncher.exe", 32, 400, &call) && call == 64);
    call = 0;
    image[800] = 'X';
    assert(fallout_startup_scan(image, sizeof(image), 0x400000, "FalloutLauncher.exe", 32, 400, &call) && !call);
    image[800] = 'F';
    put32(image + 73, UINT32_MAX);
    assert(fallout_startup_scan(image, sizeof(image), 0x400000, "FalloutLauncher.exe", 32, 400, &call) && !call);
    assert(!fallout_startup_scan(image, sizeof(image), 0x400000, "FalloutLauncher.exe", 32, SIZE_MAX, &call));
    put32(image + 73, 83);
    memcpy(image + 256, image + 64, 128);
    assert(!fallout_startup_scan(image, sizeof(image), 0x400000, "FalloutLauncher.exe", 32, 400, &call));

    memset(image, 0, sizeof(image));
    memcpy(image + 800, "FalloutNVLauncher.exe", 21);
    memcpy(image + 64, "\xe8\x00\x00\x00\x00\x0f\xb6\xc8\x85\xc9\x0f\x84", 12);
    put32(image + 76, 112);
    memcpy(image + 80, "\x6a\x01\xe8", 3);
    image[140] = 0x68;
    put32(image + 141, 0x400000 + 800);
    call = 0;
    assert(fallout_startup_scan(image, sizeof(image), 0x400000, "FalloutLauncher.exe", 32, 400, &call) && !call);
    assert(fallout_startup_scan(image, sizeof(image), 0x400000, "FalloutNVLauncher.exe", 32, 400, &call) && call == 64);
    memcpy(image + 64, fallout_startup_replacement, sizeof(fallout_startup_replacement));
    call = 0;
    assert(fallout_startup_scan(image, sizeof(image), 0x400000, "FalloutNVLauncher.exe", 32, 400, &call) && !call);
    image[64] = 0xe8;
    image[74] = 0x0f; image[75] = 0x85;
    assert(fallout_startup_scan(image, sizeof(image), 0x400000, "FalloutNVLauncher.exe", 32, 400, &call) && !call);
    image[75] = 0x84;
    put32(image + 76, 32);
    assert(fallout_startup_scan(image, sizeof(image), 0x400000, "FalloutNVLauncher.exe", 32, 400, &call) && !call);
    put32(image + 76, 112);
    assert(fallout_startup_scan(image, 808, 0x400000, "FalloutNVLauncher.exe", 32, 400, &call) && !call);
    memcpy(image + 256, image + 64, 128);
    assert(!fallout_startup_scan(image, sizeof(image), 0x400000, "FalloutNVLauncher.exe", 32, 400, &call));
    assert(argc % 2 == 1);
    for (int i = 1; i + 1 < argc; i += 2)
        check_image(argv[i + 1], fallout_startup_launcher(argv[i], 0x14c));
    puts("Fallout startup matching passed");
    return 0;
}
