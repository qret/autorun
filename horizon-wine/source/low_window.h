#ifndef WINE_NX_LOW_WINDOW_H
#define WINE_NX_LOW_WINDOW_H

#include <stdint.h>

#define WINE_NX_GUEST_BASE UINT64_C(0x200000)
#define WINE_NX_NATIVE_BASE UINT64_C(0x100000000)
#define WINE_NX_HOST_LIMIT UINT64_C(0x8000000000)

static inline int wine_nx_is_low_window( uint64_t base, uint64_t size )
{
    return base == WINE_NX_GUEST_BASE && size == WINE_NX_HOST_LIMIT - base;
}

/* Without the low window a fixed-low guest address g can still live at base + g
 * in a 39-bit process, where the translator adds the base: the alias window. It
 * covers what Horizon keeps below its 128 MiB map start. */
#define WINE_NX_ALIAS_WINDOW_SIZE UINT64_C(0x8000000)

/* Called with the libnx virtual-memory lock held. */
int wine_nx_low_window_reserve(void);
int wine_nx_low_window_probe( void (*report)( const char * ) );
int wine_nx_alias_window_probe( void (*report)( const char * ) );

#endif
