/* Copyright 2026 Wine-NX contributors. LGPL-2.1-or-later. */
/* The alias window: i386 code and data at guest 0x400000, kept on the host at
 * WINDOW + 0x400000, with the stack above 128 MiB as Wine places it. Run through
 * the interpreter or the dynarec; nothing is mapped at the guest addresses, so
 * an operand or a fetch that misses the window faults and kills the test. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>
#include "wow64_box64_engine.h"

#define WINDOW 0x100000000ull
#define IMAGE 0x400000u
#define IMAGE_SIZE 0x2000u
#define DATA (IMAGE + 0x1000u)
#define STACK 0x10000000u
#define STACK_SIZE 0x10000u
#define FUNC (IMAGE + 0x100u)
/* The completion address's page is left to the interpreter; keep it off the code page. */
#define DONE (IMAGE + 0x1f00u)
#define GATES (STACK + 0x8000u)

static unsigned char *image, *stack;

static void *guest_host( ULONG address, SIZE_T size )
{
    if (address >= IMAGE && address - IMAGE < IMAGE_SIZE && size <= IMAGE_SIZE - (address - IMAGE))
        return image + (address - IMAGE);
    if (address >= STACK && address - STACK < STACK_SIZE && size <= STACK_SIZE - (address - STACK))
        return stack + (address - STACK);
    return NULL;
}

static NTSTATUS read_guest( void *opaque, ULONG address, void *buffer, SIZE_T size )
{
    void *host = guest_host( address, size );

    (void)opaque;
    if (!host) return STATUS_ACCESS_VIOLATION;
    memcpy( buffer, host, size );
    return STATUS_SUCCESS;
}

static void init_context( I386_CONTEXT *ctx, ULONG pc, ULONG sp )
{
    XMM_SAVE_AREA32 fx = {0};
    memset( ctx, 0, sizeof(*ctx) );
    ctx->ContextFlags = CONTEXT_I386_ALL;
    ctx->Eip = pc; ctx->Esp = sp; ctx->EFlags = 0x202;
    ctx->SegCs = 0x23; ctx->SegSs = ctx->SegDs = ctx->SegEs = 0x2b; ctx->SegFs = 0x53;
    ctx->FloatSave.ControlWord = 0x37f; ctx->FloatSave.TagWord = 0xffff;
    fx.ControlWord = 0x37f; fx.MxCsr = 0x1f80;
    memcpy( ctx->ExtendedRegisters, &fx, sizeof(fx) );
}

/* A miss of the window: say where, for the one fix it needs. */
static void fault_handler( int signal, siginfo_t *info, void *context )
{
    ucontext_t *uc = context;
    printf( "FAULT signal %d at %p, pc %#llx\n", signal, info->si_addr, (unsigned long long)uc->uc_mcontext.pc );
    _exit( 128 + signal );
}

static ULONG data( ULONG offset ) { ULONG v; memcpy( &v, image + 0x1000 + offset, 4 ); return v; }

#ifdef WINE_NX_BOX64_DYNAREC
extern void wine_nx_box64_invalidate( uintptr_t address, size_t size, int destroy );
#endif

int main(void)
{
    static const unsigned char program[] = {
        0x8b,0x05,0x00,0x10,0x40,0x00,       /* mov eax, [0x401000]       absolute */
        0xbb,0x00,0x10,0x40,0x00,            /* mov ebx, 0x401000 */
        0x8b,0x4b,0x04,                      /* mov ecx, [ebx+4]           base + disp8 */
        0x01,0x4b,0x08,                      /* add [ebx+8], ecx           read-modify-write */
        0x8d,0x7b,0x10,                      /* lea edi, [ebx+0x10]        a number, not moved */
        0x89,0x07,                           /* mov [edi], eax             base only */
        0xbe,0x01,0x00,0x00,0x00,            /* mov esi, 1 */
        0x8b,0x54,0xb3,0x14,                 /* mov edx, [ebx+esi*4+0x14]  SIB with index */
        0x89,0x53,0x1c,                      /* mov [ebx+0x1c], edx */
        0x50,                                /* push eax                   stack above 128 MiB */
        0xe8,0x00,0x00,0x00,0x00,            /* call FUNC                  (rel32 below) */
        0x58,                                /* pop eax */
        0xba,0x00,0x1f,0x40,0x00,            /* mov edx, DONE */
        0xff,0xe2                            /* jmp edx */
    };
    static const unsigned char func[] = {
        0x8b,0x44,0x24,0x04,                 /* mov eax, [esp+4] */
        0xc1,0xe0,0x01,                      /* shl eax, 1                 changed below */
        0x89,0x05,0x20,0x10,0x40,0x00,       /* mov [0x401020], eax        absolute store */
        0xc3                                 /* ret */
    };
    static const ULONG values[] = { 7, 5, 100, 0, 0, 0, 0x55 };
    const struct wine_nx_wow64_gates gates = { GATES, GATES + 0x10 };
    const struct wine_nx_wow64_host host = { read_guest, NULL, NULL, NULL };
    I386_CONTEXT context;
    ULONGLONG executed;
    NTSTATUS status;
    LONG rel;
    unsigned int round;

    struct sigaction action = {0};

    setvbuf( stdout, NULL, _IONBF, 0 );
    sigemptyset( &action.sa_mask );
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = fault_handler;
    sigaction( SIGSEGV, &action, NULL );
    sigaction( SIGBUS, &action, NULL );
    assert( !wine_nx_box64_set_window( 0x300000000ull ) ); /* not a power of two */
    assert( !wine_nx_box64_set_window( 0x80000000ull ) );  /* below 4 GiB */
    assert( wine_nx_box64_set_window( WINDOW ) );
    image = mmap( (void *)(uintptr_t)(WINDOW + IMAGE), IMAGE_SIZE, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0 );
    stack = mmap( (void *)(uintptr_t)STACK, STACK_SIZE, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0 );
    if (image != (void *)(uintptr_t)(WINDOW + IMAGE) || stack != (void *)(uintptr_t)STACK)
    {
        puts( "Unable to map the window test arenas; execution was not attempted." );
        return 1;
    }
    memset( image, 0xcc, IMAGE_SIZE );
    memcpy( image, program, sizeof(program) );
    rel = (LONG)FUNC - (LONG)(IMAGE + 40);   /* the call's rel32 is at 36, the next instruction at 40 */
    memcpy( image + 36, &rel, 4 );
    memcpy( image + 0x100, func, sizeof(func) );

    for (round = 0; round < 3; round++)
    {
        memcpy( image + 0x1000, values, sizeof(values) );
        init_context( &context, IMAGE, STACK + 0x6000 );
        status = wine_nx_box64_run( &context, STACK + 0x3000, &gates, &host, NULL, DONE, 1000, &executed );
        printf( "round %u: status=%08x eax=%u ecx=%u edi=%08x [8]=%u [10]=%u [1c]=%#x [20]=%u\n", round,
                (unsigned)status, (unsigned)context.Eax, (unsigned)context.Ecx, (unsigned)context.Edi,
                (unsigned)data( 8 ), (unsigned)data( 0x10 ), (unsigned)data( 0x1c ), (unsigned)data( 0x20 ) );
        assert( status == STATUS_SUCCESS && context.Eip == DONE && context.Esp == STACK + 0x6000 );
        assert( context.Eax == 7 && context.Ecx == 5 && context.Edi == DATA + 0x10 );
        assert( data( 8 ) == 105 && data( 0x10 ) == 7 && data( 0x1c ) == 0x55 );
        assert( data( 0x20 ) == 7u << (round + 1) );
        /* Code changed in the window is reported by guest address, then by its
         * place on the host; both reach the block keyed by the guest address. */
        image[0x100 + 6] = round + 2;
#ifdef WINE_NX_BOX64_DYNAREC
        wine_nx_box64_invalidate( round ? WINDOW + FUNC : FUNC, sizeof(func), 0 );
#endif
    }
    {
        /* String instructions, moffs and XLAT reach the window too, and an operand
         * through ESI right after a string instruction lands where it should. */
        static const unsigned char strings[] = {
            0xbe,0x00,0x10,0x40,0x00,        /* mov esi, 0x401000 */
            0xbf,0x40,0x10,0x40,0x00,        /* mov edi, 0x401040 */
            0xb9,0x04,0x00,0x00,0x00,        /* mov ecx, 4 */
            0xfc,                            /* cld */
            0xf3,0xa5,                       /* rep movsd */
            0xa1,0x44,0x10,0x40,0x00,        /* mov eax, [0x401044]   moffs */
            0xa3,0x50,0x10,0x40,0x00,        /* mov [0x401050], eax   moffs */
            0xbb,0x60,0x10,0x40,0x00,        /* mov ebx, 0x401060 */
            0xb0,0x03,                       /* mov al, 3 */
            0xd7,                            /* xlat */
            0xa2,0x54,0x10,0x40,0x00,        /* mov [0x401054], al    moffs */
            0xbf,0x58,0x10,0x40,0x00,        /* mov edi, 0x401058 */
            0xb8,0x11,0x22,0x33,0x44,        /* mov eax, 0x44332211 */
            0xab,                            /* stosd */
            0xbe,0x58,0x10,0x40,0x00,        /* mov esi, 0x401058 */
            0xac,                            /* lodsb */
            0x88,0x05,0x5c,0x10,0x40,0x00,   /* mov [0x40105c], al */
            0x8b,0x0e,                       /* mov ecx, [esi]        after a string instruction */
            0x89,0x0d,0x70,0x10,0x40,0x00,   /* mov [0x401070], ecx */
            0xbf,0x58,0x10,0x40,0x00,        /* mov edi, 0x401058 */
            0xb0,0x33,                       /* mov al, 0x33 */
            0xb9,0x08,0x00,0x00,0x00,        /* mov ecx, 8 */
            0xf2,0xae,                       /* repne scasb */
            0x89,0x3d,0x74,0x10,0x40,0x00,   /* mov [0x401074], edi */
            0xba,0x00,0x1f,0x40,0x00,        /* mov edx, DONE */
            0xff,0xe2                        /* jmp edx */
        };
        static const unsigned char table[] = { 0xa0, 0xa1, 0xa2, 0xa3 };

        memcpy( image + 0x200, strings, sizeof(strings) );
        memcpy( image + 0x1000, values, sizeof(values) );
        memcpy( image + 0x1060, table, sizeof(table) );
        init_context( &context, IMAGE + 0x200, STACK + 0x6000 );
        status = wine_nx_box64_run( &context, STACK + 0x3000, &gates, &host, NULL, DONE, 1000, &executed );
        printf( "strings: status=%08x esi=%08x edi=%08x [40]=%u [50]=%u [54]=%#x [58]=%#x [5c]=%#x [70]=%#x [74]=%#x\n",
                (unsigned)status, (unsigned)context.Esi, (unsigned)context.Edi, (unsigned)data( 0x40 ),
                (unsigned)data( 0x50 ), image[0x1054], (unsigned)data( 0x58 ), image[0x105c],
                (unsigned)data( 0x70 ), (unsigned)data( 0x74 ) );
        assert( status == STATUS_SUCCESS && context.Eip == DONE );
        assert( data( 0x40 ) == 7 && data( 0x44 ) == 5 && data( 0x48 ) == 100 && data( 0x4c ) == 0 );
        assert( data( 0x50 ) == 5 && image[0x1054] == 0xa3 && data( 0x58 ) == 0x44332211 );
        assert( image[0x105c] == 0x11 && data( 0x70 ) == 0x11443322 && data( 0x74 ) == 0x40105b );
        assert( context.Esi == 0x401059 && context.Edi == 0x40105b && context.Ecx == 5 );
    }
    {
        /* BT with a register adds the bit offset's byte part to the operand. */
        static const unsigned char bits[] = {
            0xbb,0x00,0x10,0x40,0x00,        /* mov ebx, 0x401000 */
            0xb9,0x22,0x00,0x00,0x00,        /* mov ecx, 34                bit 2 of [ebx+4] */
            0x0f,0xa3,0x0b,                  /* bt [ebx], ecx */
            0x0f,0x92,0xc0,                  /* setc al */
            0xa2,0x80,0x10,0x40,0x00,        /* mov [0x401080], al */
            0xb9,0x28,0x00,0x00,0x00,        /* mov ecx, 40                bit 0 of [ebx+5] */
            0xf0,0x0f,0xab,0x0b,             /* lock bts [ebx], ecx */
            0xeb,0x03,0x90,0x90,0x90,        /* jmp over padding: the dynarec peeks at it (is_nops) */
            0xba,0x00,0x1f,0x40,0x00,        /* mov edx, DONE */
            0xff,0xe2                        /* jmp edx */
        };

        memcpy( image + 0x300, bits, sizeof(bits) );
        memcpy( image + 0x1000, values, sizeof(values) );
        image[0x1080] = 0;
        init_context( &context, IMAGE + 0x300, STACK + 0x6000 );
        status = wine_nx_box64_run( &context, STACK + 0x3000, &gates, &host, NULL, DONE, 1000, &executed );
        printf( "bits: status=%08x [80]=%u [4]=%#x\n", (unsigned)status, image[0x1080], (unsigned)data( 4 ) );
        assert( status == STATUS_SUCCESS && image[0x1080] == 1 && data( 4 ) == 0x105 );
    }
#ifdef WINE_NX_BOX64_DYNAREC
    {
        /* The window code ran translated, not handed to the interpreter. */
        extern uint64_t wine_nx_box64_dynarec_bytes;
        extern unsigned long long wine_nx_box64_native_entries;
        printf( "dynarec bytes=%llu native entries=%llu\n", (unsigned long long)wine_nx_box64_dynarec_bytes,
                wine_nx_box64_native_entries );
        assert( wine_nx_box64_dynarec_bytes > 0 && wine_nx_box64_native_entries > 0 );
    }
#endif
    puts( "Alias window: operands, read-modify-write, LEA, a call, changed code, string instructions, moffs, XLAT and BT passed" );
    return 0;
}
