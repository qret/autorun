#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'dlls/ntdll/unix/file.c').read_text()
start = source.index('static BOOL async_read_proc(')
end = source.index('\nstatic BOOL async_write_proc(', start)

fixture = r'''
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define __SWITCH__ 1
#define BOOL int
#define TRUE 1
#define FALSE 0
#define ULONG_PTR size_t
#define FILE_READ_DATA 1
#define STATUS_SUCCESS 0
#define STATUS_ALERTED 0x101
#define STATUS_TIMEOUT 0x102
#define STATUS_IO_TIMEOUT 0xc00000b5u
#define STATUS_PIPE_BROKEN 0xc000014bu
#define STATUS_BUFFER_OVERFLOW 0x80000005u
@HEADER@
struct async_fileio { int unused; };
struct async_fileio_read
{
    struct async_fileio io;
    unsigned int handle, already, count;
    char *buffer;
    BOOL avail_mode;
};
static struct horizon_byte_pipe_end *reader;
static int releases;
static unsigned int server_get_unix_fd(unsigned int handle, unsigned int access, int *fd,
                                      int *needs_close, void *type, void *options)
{
    (void)handle; (void)access; (void)type; (void)options;
    *fd = 42; *needs_close = 0;
    return 0;
}
static int horizon_is_byte_pipe(int fd) { assert(fd == 42); return 1; }
static ssize_t virtual_locked_pipe_read(int fd, void *buffer, size_t size, unsigned int *status)
{
    int more;
    ssize_t count;
    assert(fd == 42);
    count = horizon_byte_pipe_read_ex(reader, buffer, size, &more);
    *status = more ? STATUS_BUFFER_OVERFLOW : STATUS_SUCCESS;
    return count;
}
static ssize_t virtual_locked_read(int fd, void *buffer, size_t size)
{ (void)fd; (void)buffer; (void)size; abort(); }
static unsigned int errno_to_status(int error) { assert(error == EPIPE); return STATUS_PIPE_BROKEN; }
static void release_fileio(struct async_fileio *io) { (void)io; ++releases; }
@CALLBACK@
int main(void)
{
    struct horizon_byte_pipe *pipe = horizon_byte_pipe_alloc(4096, 4096);
    struct horizon_byte_pipe_end ends[2] = {{pipe, 0}, {pipe, 1}};
    struct async_fileio_read request = {0};
    char buffer[16];
    size_t info = 123;
    unsigned int status;
    reader = &ends[0];
    pipe->message_mode = pipe->read_mode[0] = 1;
    horizon_byte_pipe_connect(pipe);
    request.count = 2; request.buffer = buffer;
    status = STATUS_ALERTED;
    assert(!async_read_proc(&request, &info, &status) && !releases);
    assert(horizon_byte_pipe_write(&ends[1], "hello", 5) == 5);
    status = STATUS_ALERTED;
    assert(async_read_proc(&request, &info, &status));
    assert(status == STATUS_BUFFER_OVERFLOW && info == 2 && releases == 1 && !memcmp(buffer, "he", 2));
    request.already = 0; request.count = sizeof(buffer);
    status = STATUS_ALERTED;
    assert(async_read_proc(&request, &info, &status));
    assert(status == STATUS_SUCCESS && info == 3 && releases == 2 && !memcmp(buffer, "llo", 3));
    assert(horizon_byte_pipe_write(&ends[1], "", 0) == 0);
    request.already = 0;
    status = STATUS_ALERTED;
    assert(async_read_proc(&request, &info, &status));
    assert(status == STATUS_SUCCESS && !info && releases == 3);
    horizon_byte_pipe_close(&ends[1]);
    status = STATUS_ALERTED;
    assert(async_read_proc(&request, &info, &status));
    assert(status == STATUS_PIPE_BROKEN && !info && releases == 4);
    horizon_byte_pipe_close(&ends[0]);

    pipe = horizon_byte_pipe_alloc(4096, 4096);
    ends[0] = (struct horizon_byte_pipe_end){pipe, 0};
    ends[1] = (struct horizon_byte_pipe_end){pipe, 1};
    horizon_byte_pipe_connect(pipe);
    assert(horizon_byte_pipe_write(&ends[1], "byte", 4) == 4);
    request.already = 0; status = STATUS_ALERTED;
    assert(async_read_proc(&request, &info, &status));
    assert(status == STATUS_SUCCESS && info == 4 && !memcmp(buffer, "byte", 4));
    horizon_byte_pipe_close(&ends[0]);
    horizon_byte_pipe_close(&ends[1]);
    puts("Wine pipe read callback: pending, partial message, empty message, byte stream and broken pipe passed");
    return 0;
}
'''
fixture = (fixture.replace('@HEADER@', (root / 'dlls/ntdll/unix/horizon_byte_pipe.h').read_text())
           .replace('@CALLBACK@', source[start:end]))
with tempfile.TemporaryDirectory(prefix='wine-nx-pipe-reads-') as tmp:
    tmp = Path(tmp)
    (tmp / 'test.c').write_text(fixture)
    subprocess.run([os.environ.get('CC', 'cc'), '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-function', '-fsanitize=address,undefined', '-pthread',
                    str(tmp / 'test.c'), '-o', str(tmp / 'test')], check=True)
    print(subprocess.run([str(tmp / 'test')], check=True, capture_output=True, text=True).stdout.strip())
