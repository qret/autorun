/* Copyright 2026 Wine-NX contributors. LGPL-2.1-or-later. */
#ifndef WINE_NX_AMD64_BOX64_ENGINE_H
#define WINE_NX_AMD64_BOX64_ENGINE_H

#include "wow64_box64_bridge.h"

struct wine_nx_amd64_host
{
    NTSTATUS (*read)( void *opaque, ULONG_PTR address, void *buffer, SIZE_T size );
    BOOL (*is_native)( void *opaque, ULONG_PTR address );
    ULONG_PTR address_limit;
};

struct wine_nx_amd64_state
{
    ULONGLONG mmx[8];
};

NTSTATUS wine_nx_box64_run_amd64( AMD64_CONTEXT *context, ULONG_PTR gs_base,
                                 struct wine_nx_amd64_state *state,
                                 const struct wine_nx_amd64_host *host, void *opaque,
                                 ULONG_PTR completion, ULONGLONG budget, ULONGLONG *executed,
                                 EXCEPTION_RECORD *exception );

#endif
