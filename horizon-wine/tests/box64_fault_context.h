#ifndef WINE_NX_TEST_BOX64_FAULT_CONTEXT_H
#define WINE_NX_TEST_BOX64_FAULT_CONTEXT_H

#include <asm/sigcontext.h>
#include <ucontext.h>

/* Whether the last signal frame carried an ESR record, the only place the
 * access (read or write) is told: Linux on arm64 writes one, qemu-user, where
 * these tests also run on a PC, does not, and then every access reads as 0. */
static int fault_context_has_esr;

static ULONG capture_fault_context( const ucontext_t *uc, ARM64_NT_CONTEXT *context )
{
    const struct _aarch64_ctx *header = (const void *)uc->uc_mcontext.__reserved;
    const unsigned char *end = uc->uc_mcontext.__reserved + sizeof(uc->uc_mcontext.__reserved);
    ULONG access = 0;
    unsigned int i;

    fault_context_has_esr = 0;
    memset( context, 0, sizeof(*context) );
    for (i = 0; i < 31; i++) context->X[i] = uc->uc_mcontext.regs[i];
    context->Sp = uc->uc_mcontext.sp;
    context->Pc = uc->uc_mcontext.pc;
    context->Cpsr = uc->uc_mcontext.pstate;
    while ((const unsigned char *)(header + 1) <= end && header->size >= sizeof(*header) &&
           header->size <= (size_t)(end - (const unsigned char *)header))
    {
        if (header->magic == FPSIMD_MAGIC && header->size >= sizeof(struct fpsimd_context))
        {
            const struct fpsimd_context *fp = (const void *)header;

            memcpy( context->V, fp->vregs, sizeof(context->V) );
            context->Fpcr = fp->fpcr;
            context->Fpsr = fp->fpsr;
        }
        else if (header->magic == ESR_MAGIC && header->size >= sizeof(struct esr_context))
        {
            access = (((const struct esr_context *)header)->esr >> 6) & 1;
            fault_context_has_esr = 1;
        }
        header = (const void *)((const unsigned char *)header + header->size);
    }
    return access;
}

#endif
