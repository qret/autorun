/*
 * Making a forwarder: an application the console installs and lists on the home
 * menu, whose only job is to start the homebrew loader with Autorun's NRO, in
 * the address space the forwarder's own NPDM asks for: 39 bits, or 32 bits for
 * Win32 images linked for a fixed low address on an Atmosphere without the
 * low-window patch. The 32-bit spaces begin at 0x200000 rather than 0x8000000,
 * which is what lets such an image map; they cap the whole space at 4 GiB.
 *
 * The pieces are sphaira's (src/owo.cpp, ISC, TotalJustice), as is the loader
 * the program NCA carries (hbl/, nx-hbloader).
 */
#ifndef WINE_NX_FORWARDER_H
#define WINE_NX_FORWARDER_H

#include <stddef.h>

struct wine_nx_forwarder
{
    const char *nro_path;   /* sdmc:/switch/wine/wine-nx-runtime.nro */
    const char *args;       /* what follows the path, or NULL */
    const char *name;       /* what the home menu shows */
    const char *author;
    const unsigned char *icon;  /* a 256x256 JPEG */
    size_t icon_size;
    unsigned int game_id;
    /* 1 for the 32-bit address space without the alias region (NPDM type 2),
     * 0 for the 39-bit one. Not for game forwarders. */
    int address_space_32bit;
};

/* The application id a forwarder for this NRO and these arguments has: 39-bit,
 * or with the address space given. It comes from them alone, so the same
 * request always names the same entry -- and two spaces are two entries. */
unsigned long long wine_nx_forwarder_title_id( const char *nro_path, const char *args );
unsigned long long wine_nx_forwarder_title_id_space( const char *nro_path, const char *args, int address_space_32bit );
unsigned long long wine_nx_forwarder_game_title_id( unsigned int game_id );

/* Where the installer says what the console made of what it wrote. */
extern void (*wine_nx_forwarder_report)( const char *line );

/* Build the forwarder and install it. Returns 0, or the Result that failed;
 * step, when given, is left pointing at what was being done. */
unsigned int wine_nx_forwarder_install( const struct wine_nx_forwarder *request, const char **step );

/* What the launcher carries to build one with (generated: forwarder_embed.c).
 * The host test supplies its own, so it declares them as pointers. */
#ifndef WINE_NX_FORWARDER_EMBED_EXTERN
extern const unsigned char wine_nx_hbl_main[];
extern const size_t wine_nx_hbl_main_size;
extern const unsigned char wine_nx_game_forwarder_main[];
extern const size_t wine_nx_game_forwarder_main_size;
extern const unsigned char wine_nx_hbl_npdm[];
extern const size_t wine_nx_hbl_npdm_size;
extern const unsigned char wine_nx_icon_any[];
extern const size_t wine_nx_icon_any_size;
extern const unsigned char wine_nx_icon_32bit[];
extern const size_t wine_nx_icon_32bit_size;
/* The mark the launcher shows in the corner of its own screens. */
extern const unsigned char wine_nx_logo[];
extern const size_t wine_nx_logo_size;
#endif

#endif
