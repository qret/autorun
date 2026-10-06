#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail_message;
static void *message_alloc( size_t size )
{
    if (fail_message) return NULL;
    return malloc( size );
}
#define malloc message_alloc
#include "../../dlls/ntdll/unix/horizon_byte_pipe.h"
#undef malloc

struct packet
{
    unsigned int writer, sequence;
    unsigned char payload[128];
};

struct writer
{
    struct horizon_byte_pipe_end *end;
    unsigned int id;
};

static void *write_packets( void *arg )
{
    struct writer *writer = arg;
    struct packet packet = {0};
    unsigned int i;

    packet.writer = writer->id;
    for (i = 0; i < 4000; ++i)
    {
        packet.sequence = i;
        memset( packet.payload, (writer->id + i) & 255, sizeof(packet.payload) );
        while (horizon_byte_pipe_write( writer->end, &packet, sizeof(packet) ) == -1)
        {
            assert( errno == EAGAIN );
            sched_yield();
        }
    }
    return NULL;
}

int main(void)
{
    struct horizon_byte_pipe *pipe = horizon_byte_pipe_alloc( 4096, 4096 );
    struct horizon_byte_pipe_end ends[2] = {{pipe, 0}, {pipe, 1}};
    struct writer writers[2] = {{&ends[1], 0}, {&ends[1], 1}};
    unsigned int expected[2] = {0}, i, j;
    unsigned char data[8192], result[8192];
    pthread_t threads[2];
    struct packet packet;
    int more;
    ssize_t count;

    assert( pipe );
    pipe->message_mode = pipe->read_mode[0] = 1;
    horizon_byte_pipe_connect( pipe );
    fail_message = 1;
    assert( horizon_byte_pipe_write( &ends[1], "failure", 7 ) == -1 && errno == ENOMEM );
    assert( !pipe->incoming[0].first && !pipe->incoming[0].used && !pipe->incoming[0].charged );
    fail_message = 0;
    for (i = 0; i < sizeof(data); ++i) data[i] = i;
    assert( horizon_byte_pipe_write( &ends[1], data, sizeof(data) ) == sizeof(data) );
    assert( !horizon_pipe_message_fits( &pipe->incoming[0], 1 ) );
    assert( horizon_byte_pipe_write( &ends[1], "later", 5 ) == -1 && errno == EAGAIN );
    assert( horizon_byte_pipe_read_ex( &ends[0], result, 17, &more ) == 17 && more );
    assert( horizon_byte_pipe_read_ex( &ends[0], result + 17, sizeof(result) - 17, &more ) == sizeof(result) - 17 && !more );
    assert( !memcmp( data, result, sizeof(data) ) );
    assert( horizon_pipe_message_fits( &pipe->incoming[0], sizeof(data) ) );

    for (i = 0; i < 2; ++i) assert( !pthread_create( &threads[i], NULL, write_packets, &writers[i] ) );
    for (i = 0; i < 8000; ++i)
    {
        do
        {
            count = horizon_byte_pipe_read_ex( &ends[0], &packet, 3, &more );
            if (count == -1) { assert( errno == EAGAIN ); sched_yield(); }
        } while (count == -1);
        assert( count == 3 && more );
        assert( horizon_byte_pipe_read_ex( &ends[0], (unsigned char *)&packet + 3,
                                          sizeof(packet) - 3, &more ) == sizeof(packet) - 3 && !more );
        assert( packet.writer < 2 && packet.sequence == expected[packet.writer]++ );
        for (j = 0; j < sizeof(packet.payload); ++j)
            assert( packet.payload[j] == ((packet.writer + packet.sequence) & 255) );
    }
    for (i = 0; i < 2; ++i) assert( !pthread_join( threads[i], NULL ) && expected[i] == 4000 );
    assert( !pipe->incoming[0].used && !pipe->incoming[0].charged && !pipe->incoming[0].messages );
    assert( horizon_byte_pipe_write( &ends[1], "unread", 6 ) == 6 );
    horizon_byte_pipe_close( &ends[0] );
    assert( horizon_byte_pipe_write( &ends[1], "closed", 6 ) == -1 && errno == EPIPE );
    horizon_byte_pipe_close( &ends[1] );
    puts("message pipes: allocation failure, oversized messages, quotas, partial reads and concurrent writers passed");
    return 0;
}
