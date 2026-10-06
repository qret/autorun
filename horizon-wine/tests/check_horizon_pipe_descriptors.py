#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import tempfile

from check_horizon_async_sockets import definition, root

fixture = r'''
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int allocation_count, fail_allocation, descriptor_count, fail_descriptor, open_count, fail_device;
static void *test_calloc(size_t count, size_t size)
{
    if (++allocation_count == fail_allocation) return NULL;
    return calloc(count, size);
}
#define calloc test_calloc
@HEADER@

struct _reent { int unused; };
static int horizon_byte_pipe_device = -1;
static pthread_mutex_t horizon_byte_pipe_device_mutex = PTHREAD_MUTEX_INITIALIZER;
static const int horizon_byte_pipe_devoptab;
struct mock_handle { void *fileStruct, *slot; int open; };
static struct mock_handle handles[8];
static int AddDevice(const int *device) { (void)device; return fail_device ? -1 : 1; }
static struct mock_handle *__get_handle(int fd) { assert(handles[fd].open); return &handles[fd]; }
static int __alloc_handle(int dev)
{
    int i;
    (void)dev;
    if (++descriptor_count == fail_descriptor) return -1;
    for (i = 0; i < 8; ++i) if (!handles[i].open)
    {
        handles[i].open = 1;
        handles[i].slot = NULL;
        handles[i].fileStruct = &handles[i].slot;
        ++open_count;
        return i;
    }
    abort();
}
@CLOSE@
static int test_close(int fd)
{
    struct mock_handle *handle = __get_handle(fd);
    horizon_byte_pipe_close_r(NULL, handle->fileStruct);
    handle->open = 0;
    --open_count;
    return 0;
}
#define close test_close
@PAIR@

static void reset(void)
{
    assert(!open_count);
    horizon_byte_pipe_device = -1;
    allocation_count = descriptor_count = 0;
    fail_allocation = fail_descriptor = fail_device = 0;
}

int main(void)
{
    int fd[2], i;
    for (i = 1; i <= 3; i++)
    {
        reset();
        fail_allocation = i;
        assert(horizon_byte_pipe_pair(fd, 4096, 4096) == -1 && errno == ENOMEM);
        assert(fd[0] == -1 && fd[1] == -1 && !open_count);
    }
    for (i = 1; i <= 2; i++)
    {
        reset();
        fail_descriptor = i;
        assert(horizon_byte_pipe_pair(fd, 4096, 4096) == -1 && errno == EMFILE);
        assert(fd[0] == -1 && fd[1] == -1 && !open_count);
    }
    reset();
    fail_device = 1;
    assert(horizon_byte_pipe_pair(fd, 4096, 4096) == -1 && errno == EMFILE);
    assert(fd[0] == -1 && fd[1] == -1 && !open_count);
    reset();
    assert(!horizon_byte_pipe_pair(fd, 4096, 4096) && open_count == 2);
    test_close(fd[0]);
    test_close(fd[1]);
    assert(!open_count);
    puts("pipe descriptors: all allocation failures and endpoint cleanup passed");
    return 0;
}
'''
fixture = (fixture.replace('@HEADER@', (root / 'dlls/ntdll/unix/horizon_byte_pipe.h').read_text())
           .replace('@CLOSE@', definition('horizon_byte_pipe_close_r'))
           .replace('@PAIR@', definition('horizon_byte_pipe_pair')))
with tempfile.TemporaryDirectory(prefix='wine-nx-pipe-descriptors-') as tmp:
    tmp = Path(tmp)
    (tmp / 'test.c').write_text(fixture, errors='surrogateescape')
    subprocess.run([os.environ.get('CC', 'cc'), '-g', '-Wall', '-Werror', '-Wno-unused-function',
                    '-fsanitize=address,undefined', str(tmp / 'test.c'), '-o', str(tmp / 'test')], check=True)
    print(subprocess.run([str(tmp / 'test')], check=True, capture_output=True, text=True).stdout.strip())
