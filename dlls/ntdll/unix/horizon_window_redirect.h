/*
 * Emulation of an AArch64 memory access that native code made to a guest address
 * in the alias window.
 *
 * A fixed-low guest image lives at base + g in a 39-bit process (low_window.c,
 * cmake/Box64Core.cmake). Translated guest code adds the base itself; native code
 * that was handed a guest pointer below 128 MiB and dereferences it faults, and
 * the handler in horizon.c serves the access from base + g with this. Loads and
 * stores of one or two registers, load-acquire/store-release, exclusive pairs
 * (the store as a compare-and-swap against what the load saw), DC ZVA and
 * LD1/ST1 of one to four registers -- what newlib's string functions and
 * compiled C use. Anything else stays a fault.
 */

#ifndef __WINE_HORIZON_WINDOW_REDIRECT_H
#define __WINE_HORIZON_WINDOW_REDIRECT_H

#include <string.h>
#include "horizon_read_redirect.h"

/* What the last emulated load-exclusive of a thread saw, and where it was. */
struct horizon_exclusive
{
    unsigned long long host, value, pc;
    unsigned int bytes, valid;
};

/* Whether [host, host + size) may be read, or written when write is set. */
typedef int (*horizon_window_accessible)( unsigned long long host, unsigned long long size, int write );

static inline unsigned long long horizon_window_reg( const struct horizon_read_regs *regs, unsigned int r )
{
    return r == 31 ? 0 : regs->x[r];
}

static inline unsigned long long horizon_window_load( const unsigned char *src, unsigned int bytes )
{
    unsigned long long value = 0;

    if (bytes == 1) value = __atomic_load_n( src, __ATOMIC_RELAXED );
    else if (bytes == 2) value = __atomic_load_n( (const unsigned short *)src, __ATOMIC_RELAXED );
    else if (bytes == 4) value = __atomic_load_n( (const unsigned int *)src, __ATOMIC_RELAXED );
    else memcpy( &value, src, bytes );
    return value;
}

static inline void horizon_window_store( unsigned char *dst, unsigned long long value, unsigned int bytes )
{
    if (bytes == 1) __atomic_store_n( dst, (unsigned char)value, __ATOMIC_RELAXED );
    else if (bytes == 2) __atomic_store_n( (unsigned short *)dst, (unsigned short)value, __ATOMIC_RELAXED );
    else if (bytes == 4) __atomic_store_n( (unsigned int *)dst, (unsigned int)value, __ATOMIC_RELAXED );
    else memcpy( dst, &value, bytes );
}

/* The guest range an access touches must lie in [0x10000, limit) and be usable
 * at base + address. */
static inline unsigned char *horizon_window_host( unsigned long long address, unsigned long long total,
                                                  unsigned long long base, unsigned long long limit,
                                                  int write, horizon_window_accessible accessible )
{
    if (address < 0x10000 || address >= limit || limit - address < total) return NULL;
    if (accessible && !accessible( base + address, total, write )) return NULL;
    return (unsigned char *)(base + address);
}

static inline int horizon_window_bytes_of( unsigned int insn, int simd, int pair, unsigned int *bytes,
                                           unsigned int *sign, int *write )
{
    unsigned int size = insn >> 30, opc = (insn >> 22) & 3;

    *sign = 0;
    if (pair)
    {
        *write = !((insn >> 22) & 1);
        if (simd)
        {
            if (size == 3) return 0;
            *bytes = 4u << size;
        }
        else
        {
            if (size == 3 || (size == 1 && *write)) return 0;  /* STGP */
            *bytes = size == 2 ? 8 : 4;
            if (size == 1) *sign = 64;  /* LDPSW */
        }
        return 1;
    }
    if (simd)
    {
        if (opc <= 1) *bytes = 1u << size;
        else if (!size) *bytes = 16;
        else return 0;
        *write = !(opc & 1);
        return 1;
    }
    if (opc == 2 && size == 3) return 0;  /* PRFM */
    if (opc == 3 && size >= 2) return 0;
    *bytes = 1u << size;
    *write = !opc;
    if (opc == 2) *sign = 64;
    else if (opc == 3) *sign = 32;
    return 1;
}

static inline int horizon_redirect_window( struct horizon_read_regs *regs, unsigned int insn,
                                           unsigned long long base, unsigned long long limit,
                                           struct horizon_exclusive *exclusive,
                                           horizon_window_accessible accessible )
{
    unsigned int t = insn & 31, n = (insn >> 5) & 31, t2 = (insn >> 10) & 31;
    unsigned int simd = (insn >> 26) & 1, bytes = 0, sign = 0, count = 1, i;
    unsigned long long address = n == 31 ? regs->sp : regs->x[n], next = address;
    unsigned char *host;
    int write = 0, writeback = 0;

    if ((insn & 0xffffffe0) == 0xd50b7420)  /* DC ZVA, Xt */
    {
        unsigned long long dczid, block;

        __asm__( "mrs %0, dczid_el0" : "=r"(dczid) );
        if (dczid & 16) return 0;
        block = 4ull << (dczid & 15);
        address = horizon_window_reg( regs, t ) & ~(block - 1);
        if (!(host = horizon_window_host( address, block, base, limit, 1, accessible ))) return 0;
        memset( host, 0, block );
        regs->pc += 4;
        return 1;
    }

    if ((insn & 0x3f000000) == 0x08000000)  /* load/store exclusive, acquire/release */
    {
        unsigned int o2 = (insn >> 23) & 1, load = (insn >> 22) & 1, o1 = (insn >> 21) & 1;
        unsigned int s = (insn >> 16) & 31, value_reg = t;

        if (o1) return 0;  /* pairs and compare-and-swap */
        bytes = 1u << (insn >> 30);
        if (!(host = horizon_window_host( address, bytes, base, limit, !load, accessible ))) return 0;
        if (load)
        {
            unsigned long long value = horizon_window_load( host, bytes );

            if (!o2)  /* LDXR, LDAXR: remember what was seen */
            {
                /* The same load again with no store-exclusive fault between: the
                 * store failed on the monitor the emulated load never set, without
                 * touching memory, and would forever. Leave it a fault. */
                if (exclusive->valid && exclusive->pc == regs->pc) return 0;
                exclusive->pc = regs->pc;
                exclusive->host = (unsigned long long)host;
                exclusive->value = value;
                exclusive->bytes = bytes;
                exclusive->valid = 1;
            }
            __atomic_thread_fence( __ATOMIC_ACQUIRE );
            if (value_reg != 31) regs->x[value_reg] = value;
        }
        else if (o2)  /* STLR */
        {
            __atomic_thread_fence( __ATOMIC_RELEASE );
            horizon_window_store( host, horizon_window_reg( regs, value_reg ), bytes );
        }
        else  /* STXR, STLXR: succeed only if the location still holds what LDXR saw */
        {
            unsigned long long value = horizon_window_reg( regs, value_reg ), expected = exclusive->value;
            int done = 0;

            if (s == t || s == n) return 0;
            if (exclusive->valid && exclusive->host == (unsigned long long)host && exclusive->bytes == bytes)
            {
                if (bytes == 1)
                {
                    unsigned char e = expected;
                    done = __atomic_compare_exchange_n( host, &e, (unsigned char)value, 0,
                                                        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
                }
                else if (bytes == 2)
                {
                    unsigned short e = expected;
                    done = __atomic_compare_exchange_n( (unsigned short *)host, &e, (unsigned short)value, 0,
                                                        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
                }
                else if (bytes == 4)
                {
                    unsigned int e = expected;
                    done = __atomic_compare_exchange_n( (unsigned int *)host, &e, (unsigned int)value, 0,
                                                        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
                }
                else done = __atomic_compare_exchange_n( (unsigned long long *)host, &expected, value, 0,
                                                         __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
            }
            exclusive->valid = 0;
            if (s != 31) regs->x[s] = !done;
        }
        regs->pc += 4;
        return 1;
    }

    if ((insn & 0xbfbf0000) == 0x0c000000 || (insn & 0xbfa00000) == 0x0c800000)  /* LD1/ST1 */
    {
        unsigned int opcode = (insn >> 12) & 15, q = (insn >> 30) & 1, load = (insn >> 22) & 1, m = (insn >> 16) & 31;
        unsigned int width = q ? 16 : 8;

        if (opcode == 7) count = 1;
        else if (opcode == 10) count = 2;
        else if (opcode == 6) count = 3;
        else if (opcode == 2) count = 4;
        else return 0;
        if (!(host = horizon_window_host( address, width * count, base, limit, !load, accessible ))) return 0;
        for (i = 0; i < count; i++)
        {
            unsigned int r = (t + i) & 31;

            if (load)
            {
                unsigned __int128 value = 0;

                memcpy( &value, host + i * width, width );
                regs->v[r] = value;
            }
            else memcpy( host + i * width, &regs->v[r], width );
        }
        if (insn & 0x00800000)  /* post-index */
        {
            next = address + (m == 31 ? width * count : horizon_window_reg( regs, m ));
            if (n == 31) regs->sp = next;
            else regs->x[n] = next;
        }
        regs->pc += 4;
        return 1;
    }

    if ((insn & 0x3a000000) == 0x28000000)  /* LDP, STP, LDNP, STNP, LDPSW */
    {
        unsigned int index = (insn >> 23) & 3;
        long long imm;

        if (!horizon_window_bytes_of( insn, simd, 1, &bytes, &sign, &write )) return 0;
        if (!index && sign) return 0;
        if (!write && t == t2) return 0;
        count = 2;
        imm = horizon_read_sext( (insn >> 15) & 0x7f, 7 ) * (long long)bytes;
        if (index != 1) address += imm;
        next = (n == 31 ? regs->sp : regs->x[n]) + imm;
        writeback = index == 1 || index == 3;
        if (writeback && !simd && n != 31 && (n == t || n == t2)) return 0;
    }
    else if ((insn & 0x3a000000) == 0x38000000)  /* LDR, STR and their variants */
    {
        if (!horizon_window_bytes_of( insn, simd, 0, &bytes, &sign, &write )) return 0;
        if (insn & 0x01000000)  /* unsigned scaled offset */
            address += ((insn >> 10) & 0xfff) * (unsigned long long)bytes;
        else if (!(insn & 0x00200000))  /* 9-bit offset: unscaled, post-index, unprivileged, pre-index */
        {
            unsigned int mode = (insn >> 10) & 3;
            long long imm = horizon_read_sext( (insn >> 12) & 0x1ff, 9 );

            if (simd && mode == 2) return 0;
            next = address + imm;
            if (mode != 1) address += imm;
            writeback = mode == 1 || mode == 3;
            if (writeback && !simd && n != 31 && n == t) return 0;
        }
        else if (((insn >> 10) & 3) == 2)  /* register offset */
        {
            unsigned int m = (insn >> 16) & 31, option = (insn >> 13) & 7;
            unsigned long long index = horizon_window_reg( regs, m );

            if (option == 2) index = (unsigned int)index;
            else if (option == 6) index = horizon_read_sext( index, 32 );
            else if (option != 3 && option != 7) return 0;
            if (insn & 0x1000) index *= bytes;
            address += index;
        }
        else return 0;
    }
    else return 0;

    if (!(host = horizon_window_host( address, (unsigned long long)bytes * count, base, limit, write, accessible )))
        return 0;
    for (i = 0; i < count; i++)
    {
        unsigned char *p = host + i * bytes;
        unsigned int r = i ? t2 : t;

        if (write)
        {
            if (simd) memcpy( p, &regs->v[r], bytes );
            else horizon_window_store( p, horizon_window_reg( regs, r ), bytes );
        }
        else if (simd)
        {
            unsigned __int128 value = 0;

            memcpy( &value, p, bytes );  /* clears the rest of the register, as a load does */
            regs->v[r] = value;
        }
        else
        {
            unsigned long long value = horizon_window_load( p, bytes );

            if (sign) value = horizon_read_sext( value, bytes * 8 );
            if (sign == 32) value = (unsigned int)value;
            if (r != 31) regs->x[r] = value;
        }
    }
    if (writeback)
    {
        if (n == 31) regs->sp = next;
        else regs->x[n] = next;
    }
    regs->pc += 4;
    return 1;
}

#endif /* __WINE_HORIZON_WINDOW_REDIRECT_H */
