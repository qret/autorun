#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <switch.h>
#include "low_window.h"

extern VirtmemReservation *__real_virtmemAddReservation( void *address, size_t size );
extern void *__real_virtmemFindAslr( size_t size, size_t guard );
extern void *__real_virtmemFindCodeMemory( size_t size, size_t guard );

static int profile_checked, low_window;
static VirtmemReservation *guest_reservation;

int wine_nx_low_window_reserve(void)
{
    u64 base, size;

    if (!profile_checked)
    {
        if (R_FAILED(svcGetInfo( &base, InfoType_AslrRegionAddress, CUR_PROCESS_HANDLE, 0 )) ||
            R_FAILED(svcGetInfo( &size, InfoType_AslrRegionSize, CUR_PROCESS_HANDLE, 0 ))) return 0;
        low_window = wine_nx_is_low_window( base, size );
        profile_checked = 1;
    }
    if (!low_window) return 1;
    if (!guest_reservation)
        guest_reservation = __real_virtmemAddReservation( (void *)WINE_NX_GUEST_BASE,
                                                         WINE_NX_NATIVE_BASE - WINE_NX_GUEST_BASE );
    return guest_reservation != NULL;
}

void *__wrap_virtmemFindAslr( size_t size, size_t guard )
{
    if (!wine_nx_low_window_reserve()) return NULL;
    return __real_virtmemFindAslr( size, guard );
}

void *__wrap_virtmemFindCodeMemory( size_t size, size_t guard )
{
    if (!wine_nx_low_window_reserve()) return NULL;
    return __real_virtmemFindCodeMemory( size, guard );
}

static int check_native_layout(void)
{
    static const InfoType regions[] = { InfoType_HeapRegionAddress, InfoType_AliasRegionAddress,
                                       InfoType_StackRegionAddress };
    u64 base;
    unsigned int i;

    if ((uintptr_t)check_native_layout < WINE_NX_NATIVE_BASE ||
        (uintptr_t)&base < WINE_NX_NATIVE_BASE || (uintptr_t)armGetTls() < WINE_NX_NATIVE_BASE) return 0;
    for (i = 0; i < sizeof(regions) / sizeof(regions[0]); i++)
        if (R_FAILED(svcGetInfo( &base, regions[i], CUR_PROCESS_HANDLE, 0 )) ||
            base < WINE_NX_NATIVE_BASE) return 0;
    return 1;
}

static int range_unmapped( uintptr_t address, uintptr_t end )
{
    MemoryInfo info;
    u32 page_info;

    while (address < end)
    {
        if (R_FAILED(svcQueryMemory( &info, &page_info, address )) || info.type != MemType_Unmapped ||
            info.addr > address || !info.size || info.size > UINT64_MAX - info.addr ||
            info.addr + info.size <= address) return 0;
        address = info.addr + info.size;
    }
    return 1;
}

static int check_guest_window(void)
{
    return range_unmapped( WINE_NX_GUEST_BASE, WINE_NX_NATIVE_BASE );
}

static int probe_page( uintptr_t address, int executable, uintptr_t min_backing, const char *tag,
                       char *message, size_t message_size )
{
    const size_t size = 0x1000;
    Handle process = envGetOwnProcessHandle();
    MemoryInfo info;
    u32 page_info, *backing = memalign( size, size );
    Result rc, unmap_rc;
    int passed = 0;

    if (!backing || (uintptr_t)backing < min_backing)
    {
        free( backing );
        snprintf( message, message_size, "%s high backing allocation failed", tag );
        return 0;
    }
    backing[0] = 0x528009a0; /* mov w0, #77 */
    backing[1] = 0xd65f03c0; /* ret */
    armDCacheFlush( backing, size );
    rc = svcMapProcessCodeMemory( process, address, (u64)backing, size );
    if (R_SUCCEEDED(rc))
    {
        rc = svcSetProcessMemoryPermission( process, address, size, executable ? Perm_Rx : Perm_Rw );
        if (R_SUCCEEDED(rc)) rc = svcQueryMemory( &info, &page_info, address );
        if (R_SUCCEEDED(rc) && info.perm == (u32)(executable ? Perm_Rx : Perm_Rw))
        {
            if (executable)
            {
                armICacheInvalidate( (void *)address, size );
                passed = ((unsigned int (*)(void))address)() == 77;
            }
            else
            {
                *(volatile u32 *)address = 0x12345678;
                passed = *(volatile u32 *)address == 0x12345678;
            }
        }
        unmap_rc = svcUnmapProcessCodeMemory( process, address, (u64)backing, size );
        if (R_FAILED(unmap_rc))
        {
            snprintf( message, message_size, "%s unmap %08llx failed: 0x%x",
                      tag, (unsigned long long)address, unmap_rc );
            return 0;
        }
        if (!executable && passed) passed = backing[0] == 0x12345678;
    }
    free( backing );
    snprintf( message, message_size, "%s %s at %08llx: %s (rc=0x%x)",
              tag, executable ? "RX execute" : "RW alias", (unsigned long long)address,
              passed ? "PASS" : "FAIL", rc );
    return passed;
}

#define ALIAS_GIB UINT64_C(0x40000000)

struct alias_layout
{
    u64 aslr_base, aslr_end, tls;
    u64 avoid[4][2];
};

static int alias_window_layout( struct alias_layout *layout )
{
    static const InfoType regions[][2] = { { InfoType_HeapRegionAddress, InfoType_HeapRegionSize },
                                           { InfoType_AliasRegionAddress, InfoType_AliasRegionSize },
                                           { InfoType_StackRegionAddress, InfoType_StackRegionSize } };
    u64 base, size, tls = (uintptr_t)armGetTls();
    unsigned int i;

    if (R_FAILED(svcGetInfo( &base, InfoType_AslrRegionAddress, CUR_PROCESS_HANDLE, 0 )) ||
        R_FAILED(svcGetInfo( &size, InfoType_AslrRegionSize, CUR_PROCESS_HANDLE, 0 ))) return 0;
    layout->aslr_base = base;
    layout->aslr_end = base + size;
    layout->tls = tls;
    for (i = 0; i < sizeof(regions) / sizeof(regions[0]); i++)
    {
        if (R_FAILED(svcGetInfo( &base, regions[i][0], CUR_PROCESS_HANDLE, 0 )) ||
            R_FAILED(svcGetInfo( &size, regions[i][1], CUR_PROCESS_HANDLE, 0 ))) return 0;
        layout->avoid[i][0] = base;
        layout->avoid[i][1] = base + size;
    }
    /* The kernel keeps thread-local pages in a 64 GiB region of its own, which
     * svcGetInfo does not report. It holds this thread's page, so nothing within
     * 64 GiB of that page on either side is known to be free of it. */
    layout->avoid[3][0] = tls > 64 * ALIAS_GIB ? tls - 64 * ALIAS_GIB : 0;
    layout->avoid[3][1] = tls + 64 * ALIAS_GIB;
    return 1;
}

static int alias_window_fits( const struct alias_layout *layout, u64 base )
{
    u64 end = base + WINE_NX_ALIAS_WINDOW_SIZE;
    unsigned int i;

    if (base < layout->aslr_base || end > layout->aslr_end) return 0;
    for (i = 0; i < sizeof(layout->avoid) / sizeof(layout->avoid[0]); i++)
        if (base < layout->avoid[i][1] && layout->avoid[i][0] < end) return 0;
    return range_unmapped( base, end );
}

/* A base that is a power of two lets the translator add it without touching
 * the flags, which hold the guest's; any other multiple of 4 GiB still gives
 * the guest address back when a pointer is cut to 32 bits. */
static int alias_window_choose( const struct alias_layout *layout, u64 *base, unsigned int *tried )
{
    u64 multiple;
    unsigned int shift;

    *tried = 0;
    for (shift = 32; shift < 39; shift++)
    {
        ++*tried;
        if (alias_window_fits( layout, UINT64_C(1) << shift ))
        {
            *base = UINT64_C(1) << shift;
            return 1;
        }
    }
    for (multiple = 3; (multiple << 32) < layout->aslr_end; multiple++)
    {
        if (!(multiple & (multiple - 1))) continue;
        ++*tried;
        if (alias_window_fits( layout, multiple << 32 ))
        {
            *base = multiple << 32;
            return 1;
        }
    }
    return 0;
}

int wine_nx_alias_window_probe( void (*report)( const char * ) )
{
    struct alias_layout layout;
    char message[256], execute_message[192] = "", write_message[192] = "";
    unsigned int tried = 0;
    u64 base = 0;
    int passed;

    virtmemLock();
    if (!alias_window_layout( &layout ))
    {
        virtmemUnlock();
        report( "[WINDOW] FAIL: the kernel did not report the address space layout" );
        return 0;
    }
    if (!alias_window_choose( &layout, &base, &tried ))
    {
        virtmemUnlock();
        snprintf( message, sizeof(message), "[WINDOW] FAIL: no free base among %u candidates; "
                  "fixed-low titles stay with Autorun 32-bit", tried );
        report( message );
        return 0;
    }
    /* Both ends of the window, the way an image and its data would use them;
     * the pages are given back at once. */
    passed = probe_page( base + 0x400000, 1, 0, "[WINDOW]", execute_message, sizeof(execute_message) ) &&
             probe_page( base + WINE_NX_ALIAS_WINDOW_SIZE - 0x10000, 0, 0, "[WINDOW]",
                         write_message, sizeof(write_message) );
    passed = passed && range_unmapped( base, base + WINE_NX_ALIAS_WINDOW_SIZE );
    virtmemUnlock();
    snprintf( message, sizeof(message), "[WINDOW] base %010llx (%s) after %u candidates; "
              "heap %010llx alias %010llx stack %010llx tls %010llx",
              (unsigned long long)base, base & (base - 1) ? "multiple of 4 GiB" : "power of two", tried,
              (unsigned long long)layout.avoid[0][0], (unsigned long long)layout.avoid[1][0],
              (unsigned long long)layout.avoid[2][0], (unsigned long long)layout.tls );
    report( message );
    if (execute_message[0]) report( execute_message );
    if (write_message[0]) report( write_message );
    report( passed ? "[WINDOW] PASS: room for the alias window; fixed-low titles still go to Autorun 32-bit" :
                     "[WINDOW] FAIL: the kernel refused pages in the alias window" );
    return passed;
}

int wine_nx_low_window_probe( void (*report)( const char * ) )
{
    int reserved, passed;
    char execute_message[192] = "", alias_message[192] = "";

    virtmemLock();
    reserved = wine_nx_low_window_reserve();
    if (!low_window)
    {
        virtmemUnlock();
        report( "[LOWVA] stock layout; fixed-low Win32 titles go to Autorun 32-bit" );
        return 0;
    }
    if (!reserved || !check_native_layout() || !check_guest_window())
    {
        virtmemUnlock();
        report( "[LOWVA] FAIL: native layout or guest reservation; reinstall the main forwarder" );
        return 0;
    }
    passed = probe_page( 0x00400000, 1, WINE_NX_NATIVE_BASE, "[LOWVA]", execute_message, sizeof(execute_message) ) &&
             probe_page( 0xffff0000, 0, WINE_NX_NATIVE_BASE, "[LOWVA]", alias_message, sizeof(alias_message) );
    passed = passed && check_guest_window();
    virtmemUnlock();
    if (execute_message[0]) report( execute_message );
    if (alias_message[0]) report( alias_message );
    report( passed ? "[LOWVA] PASS: low window reserved for Wine; native allocations above 4 GiB" :
                     "[LOWVA] FAIL: fixed-low Win32 titles cannot start in this process" );
    return passed;
}
