/* Copyright 2026 Wine-NX contributors. LGPL-2.1-or-later. */
#ifndef WINE_NX_WOW64_BOX64_ENGINE_H
#define WINE_NX_WOW64_BOX64_ENGINE_H
#include <stdint.h>
#include "wow64_box64_bridge.h"

/* Experimental interpreter entry. Guest pointers are identity mapped; the
 * host exception handler must call wine_nx_box64_handle_fault for unresolved
 * data aborts. Compiler read faults discard the translation and retry through
 * the interpreter. A nonzero completion PC
 * is for embedding/tests, not a guest-accessible native function pointer.
 * Supports integer/segment/SSE transfer with default MXCSR; x87/AVX and
 * nonempty x87 state are rejected explicitly. This is not a full CPU backend.
 * With both dispatch callbacks NULL, returns success at a gate without changing
 * its stack/EAX; the PE caller dispatches and invokes the engine again.
 * Each invocation owns its emulator, allowing native callbacks to reenter. */
NTSTATUS wine_nx_box64_run( I386_CONTEXT *context, ULONG fs_base,
                          const struct wine_nx_wow64_gates *gates,
                          const struct wine_nx_wow64_host *host, void *opaque,
                          ULONG completion_pc, ULONGLONG budget, ULONGLONG *executed );

/* Called only for unresolved native memory faults. Returns FALSE for a native
 * address or an inactive interpreter; during block compilation it returns to
 * Box64's interpreter fallback. For a 32-bit address during Run it exits
 * the active run with STATUS_ACCESS_VIOLATION and the context of the faulting
 * instruction, which WoW64 raises into the guest's own handlers: in
 * translated code its registers come from the native ones (x, x0-x30 as the
 * fault left them, and the native pc), as Box64's signal handler takes them.
 * access is 0 for a read, 1 for a write. Horizon calls this after libnx has
 * returned from the kernel exception, Linux test hosts from a signal handler. */
BOOL wine_nx_box64_handle_fault( ULONG_PTR address, ULONG access, ULONG_PTR pc,
                                 const unsigned long long *x );

/* What the last STATUS_ACCESS_VIOLATION of this thread's run was about: the
 * address, and 0 read, 1 write or 8 execute. */
void wine_nx_box64_last_fault( ULONG *address, ULONG *access );

/* The alias window: guest addresses below 128 MiB live at base + address, and
 * translated 32-bit code reaches them there. base is a power of two of at
 * least 4 GiB, so a pointer cut to 32 bits is the guest address again, or 0
 * for no window. Set it before the first 32-bit code runs; blocks translated
 * before it do not move their operands. Returns FALSE for any other base. */
BOOL wine_nx_box64_set_window( uintptr_t base );
#endif
