/* Host test for dlls/ntdll/unix/horizon_window_redirect.h, on AArch64 Linux (qemu-user
 * does). Each snippet loads and stores relative to a base in x1 (index in x2) and
 * stores its registers to out[]. It runs against real memory, then against a guest
 * address with nothing mapped, whose faults the header serves from the same bytes
 * kept higher, as the alias window does: registers, writeback and memory must come
 * out the same. */
#if !defined(__aarch64__) || !defined(__linux__)
#include <stdio.h>

int main(void)
{
    puts( "horizon_window_redirect: skipped (needs AArch64 Linux or qemu-user)" );
    return 0;
}
#else
#define _GNU_SOURCE
#include <asm/sigcontext.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

#include "../../dlls/ntdll/unix/horizon_window_redirect.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define BASE 0x100000000ull   /* the window's base */
#define GUEST 0x400000ull     /* a guest address in the window */
#define SIZE 0x2000ull
#define LIMIT 0x8000000ull

/* x3..x6 and q1..q4 get known values first; out[] = x3, x4, x5, x6, q1 lo/hi, q2 lo/hi,
 * q3 lo/hi, q4 lo/hi, x1, x10. */
#define SNIPPET(name, body) \
    __asm__( ".text\n.p2align 2\n.globl " #name "\n" #name ":\n" \
             "movz x3, #0x1111\nmovk x3, #0x2222, lsl #16\nmovk x3, #0x3333, lsl #32\nmovk x3, #0x4444, lsl #48\n" \
             "movz x4, #0x5555\nmovk x4, #0x6666, lsl #16\nmovk x4, #0x7777, lsl #32\nmovk x4, #0x8888, lsl #48\n" \
             "mov x5, #-1\nmov x6, #-1\nmov x10, #0\n" \
             "dup v1.2d, x3\ndup v2.2d, x4\nmovi v3.2d, #0xffffffffffffffff\nmovi v4.2d, #0xffffffffffffffff\n" \
             body \
             "stp x3, x4, [x0]\nstp x5, x6, [x0, #16]\nstp q1, q2, [x0, #32]\nstp q3, q4, [x0, #64]\n" \
             "stp x1, x10, [x0, #96]\nret\n" ); \
    extern void name( unsigned long long *out, unsigned long long base, unsigned long long index );

SNIPPET(str_x_u12,      "str x3, [x1, #0x320]\n")
SNIPPET(str_w_u12,      "str w4, [x1, #0x324]\n")
SNIPPET(strb_strh,      "strb w3, [x1, #0x2d4]\nstrh w4, [x1, #0x26a]\n")
SNIPPET(stur_neg,       "add x1, x1, #0x400\nstur x3, [x1, #-0x100]\n")
SNIPPET(str_pre_post,   "str x3, [x1, #0x40]!\nstr w4, [x1], #0x20\n")
SNIPPET(str_reg,        "str x3, [x1, x2, lsl #3]\nstrb w4, [x1, x2]\n")
SNIPPET(str_d_q,        "str d1, [x1, #0x3c8]\nstr q2, [x1, #0x3d0]\n")
SNIPPET(stp_x,          "stp x3, x4, [x1, #16]\n")
SNIPPET(stp_w_pre,      "stp w3, w4, [x1, #8]!\n")
SNIPPET(stp_q_post,     "stp q1, q2, [x1], #64\n")
SNIPPET(stnp_ldnp,      "stnp x3, x4, [x1, #32]\nldnp x5, x6, [x1, #32]\n")
SNIPPET(ldp_ldpsw,      "ldp x5, x6, [x1, #0x100]\nldpsw x3, x4, [x1, #0xf0]\n")
SNIPPET(ldr_mix,        "ldr x5, [x1, #0x320]\nldrsb x6, [x1, #0x2d5]\nldr q3, [x1, #0x3d0]\n")
SNIPPET(acquire_release,"add x10, x1, #0x80\nldar w5, [x10]\nstlr w3, [x10]\n")
SNIPPET(ld1_st1,        "ld1 {v3.16b}, [x1]\nadd x10, x1, #0x200\nst1 {v1.16b, v2.16b}, [x10]\n")
SNIPPET(ld1_post,       "ld1 {v3.2d, v4.2d}, [x1], #32\n")
SNIPPET(st1_post_reg,   "mov x10, #48\nst1 {v1.8b}, [x1], x10\n")

typedef void (*snippet)( unsigned long long *, unsigned long long, unsigned long long );
static const struct { const char *name; snippet run; } tests[] = {
    {"str_x_u12", str_x_u12}, {"str_w_u12", str_w_u12}, {"strb_strh", strb_strh},
    {"stur_neg", stur_neg}, {"str_pre_post", str_pre_post}, {"str_reg", str_reg},
    {"str_d_q", str_d_q}, {"stp_x", stp_x}, {"stp_w_pre", stp_w_pre}, {"stp_q_post", stp_q_post},
    {"stnp_ldnp", stnp_ldnp}, {"ldp_ldpsw", ldp_ldpsw}, {"ldr_mix", ldr_mix},
    {"acquire_release", acquire_release}, {"ld1_st1", ld1_st1}, {"ld1_post", ld1_post},
    {"st1_post_reg", st1_post_reg},
};

static unsigned char *host;
static struct horizon_exclusive exclusive;
static unsigned int served;

static int accessible( unsigned long long address, unsigned long long size, int write )
{
    (void)write;
    return address >= BASE + GUEST && address + size <= BASE + GUEST + SIZE;
}

static struct fpsimd_context *fpsimd_of( ucontext_t *uc )
{
    struct _aarch64_ctx *head = (struct _aarch64_ctx *)uc->uc_mcontext.__reserved;

    while (head->magic && head->magic != FPSIMD_MAGIC) head = (struct _aarch64_ctx *)((char *)head + head->size);
    return head->magic ? (struct fpsimd_context *)head : NULL;
}

static void fault_handler( int signal, siginfo_t *info, void *context )
{
    ucontext_t *uc = context;
    struct fpsimd_context *fp = fpsimd_of( uc );
    struct horizon_read_regs regs;
    unsigned int i;

    (void)signal;
    if (!fp || (unsigned long long)info->si_addr - GUEST >= SIZE) _exit( 2 );
    for (i = 0; i < 31; i++) regs.x[i] = uc->uc_mcontext.regs[i];
    regs.sp = uc->uc_mcontext.sp;
    regs.pc = uc->uc_mcontext.pc;
    for (i = 0; i < 32; i++) regs.v[i] = fp->vregs[i];
    if (!horizon_redirect_window( &regs, *(const unsigned int *)regs.pc, BASE, LIMIT, &exclusive, accessible ))
    {
        printf( "not served: insn %08x at pc %#llx, far %p\n", *(const unsigned int *)regs.pc, regs.pc, info->si_addr );
        _exit( 3 );
    }
    served++;
    for (i = 0; i < 31; i++) uc->uc_mcontext.regs[i] = regs.x[i];
    uc->uc_mcontext.sp = regs.sp;
    uc->uc_mcontext.pc = regs.pc;
    for (i = 0; i < 32; i++) fp->vregs[i] = regs.v[i];
}

static void fill( unsigned char *p )
{
    unsigned int i;
    for (i = 0; i < SIZE; i++) p[i] = (unsigned char)(i * 37 + 11);
}

int main(void)
{
    static unsigned char real[SIZE] __attribute__((aligned(4096)));
    static unsigned char after_real[SIZE];
    struct sigaction action = {0};
    unsigned long long out_real[14], out_window[14];
    unsigned int i, failed = 0;

    setvbuf( stdout, NULL, _IONBF, 0 );
    host = mmap( (void *)(BASE + GUEST), SIZE, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0 );
    if (host != (void *)(BASE + GUEST))
    {
        puts( "horizon_window_redirect: unable to map the window" );
        return 1;
    }
    sigemptyset( &action.sa_mask );
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = fault_handler;
    sigaction( SIGSEGV, &action, NULL );

    for (i = 0; i < ARRAY_SIZE(tests); i++)
    {
        unsigned int before = served;

        fill( real );
        tests[i].run( out_real, (unsigned long long)real, 5 );
        memcpy( after_real, real, SIZE );
        out_real[12] -= (unsigned long long)real;  /* x1, and x10 where it holds an address */
        if (out_real[13] - (unsigned long long)real < SIZE) out_real[13] -= (unsigned long long)real;

        fill( host );
        exclusive.valid = 0;
        tests[i].run( out_window, GUEST, 5 );
        out_window[12] -= GUEST;
        if (out_window[13] - GUEST < SIZE) out_window[13] -= GUEST;

        if (memcmp( out_real, out_window, sizeof(out_real) ) || memcmp( after_real, host, SIZE ) || served == before)
        {
            unsigned int r;
            printf( "FAIL %s (served %u)\n", tests[i].name, served - before );
            for (r = 0; r < 14; r++)
                if (out_real[r] != out_window[r])
                    printf( "  out[%u] real %#llx window %#llx\n", r, out_real[r], out_window[r] );
            for (r = 0; r < SIZE; r++)
                if (after_real[r] != host[r])
                {
                    printf( "  first memory difference at +%#x: real %02x window %02x\n", r, after_real[r], host[r] );
                    break;
                }
            failed++;
        }
    }
    {
        /* DC ZVA through the emulator directly: qemu-user does not deliver its fault
         * on an unmapped page. */
        struct horizon_read_regs regs = {0};
        unsigned long long dczid;

        __asm__( "mrs %0, dczid_el0" : "=r"(dczid) );
        fill( host );
        regs.x[10] = GUEST + 0x48;
        regs.pc = 0x1000;
        if (!(dczid & 16))
        {
            unsigned long long block = 4ull << (dczid & 15), start = 0x48 & ~(block - 1), j;

            assert( horizon_redirect_window( &regs, 0xd50b742a, BASE, LIMIT, &exclusive, accessible ) );
            assert( regs.pc == 0x1004 );
            for (j = 0; j < SIZE; j++)
                assert( host[j] == ((j >= start && j < start + block) ? 0 : (unsigned char)(j * 37 + 11)) );
        }
        /* Exclusive pairs through the emulator directly: qemu-user fails a
         * store-exclusive whose monitor is not set without touching memory, so its
         * fault never comes. */
        {
            unsigned int value;

            fill( host );
            memcpy( &value, host, 4 );
            regs.x[1] = GUEST;
            regs.pc = 0x2000;
            exclusive.valid = 0;
            assert( horizon_redirect_window( &regs, 0x885f7c25, BASE, LIMIT, &exclusive, accessible ) );  /* ldxr w5, [x1] */
            assert( regs.x[5] == value && exclusive.valid && regs.pc == 0x2004 );
            regs.x[5] += 7;
            assert( horizon_redirect_window( &regs, 0x88067c25, BASE, LIMIT, &exclusive, accessible ) );  /* stxr w6, w5, [x1] */
            assert( regs.x[6] == 0 && !exclusive.valid );
            memcpy( &value, host, 4 );
            assert( value == (unsigned int)regs.x[5] );
            /* no load-exclusive before it: the store fails and writes nothing */
            regs.x[5] = 0x12345678;
            assert( horizon_redirect_window( &regs, 0x88067c25, BASE, LIMIT, &exclusive, accessible ) );
            assert( regs.x[6] == 1 );
            memcpy( &value, host, 4 );
            assert( value != 0x12345678 );
            /* memory changed between the two: the store fails */
            regs.pc = 0x2000;
            assert( horizon_redirect_window( &regs, 0xc85ffc25, BASE, LIMIT, &exclusive, accessible ) );  /* ldaxr x5, [x1] */
            host[0] ^= 1;
            regs.x[5] += 1;
            assert( horizon_redirect_window( &regs, 0xc806fc25, BASE, LIMIT, &exclusive, accessible ) );  /* stlxr w6, x5, [x1] */
            assert( regs.x[6] == 1 );
            /* the same load-exclusive again with no store fault between: refused */
            regs.pc = 0x2000;
            assert( horizon_redirect_window( &regs, 0x885f7c25, BASE, LIMIT, &exclusive, accessible ) );
            regs.pc = 0x2000;
            assert( !horizon_redirect_window( &regs, 0x885f7c25, BASE, LIMIT, &exclusive, accessible ) );
        }
        /* below the window: not served */
        regs.x[10] = 0x8000;
        assert( !horizon_redirect_window( &regs, 0xd50b742a, BASE, LIMIT, &exclusive, accessible ) );
    }
    if (failed) return 1;
    printf( "Window redirect: %u snippets, %u faults served; loads, stores, pairs, "
            "acquire/release and LD1/ST1 matched real memory; DC ZVA and exclusive pairs emulated\n", (unsigned)ARRAY_SIZE(tests), served );
    return 0;
}
#endif
