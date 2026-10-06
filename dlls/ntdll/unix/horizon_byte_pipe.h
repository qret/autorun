#ifndef WINE_NX_HORIZON_BYTE_PIPE_H
#define WINE_NX_HORIZON_BYTE_PIPE_H

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

struct horizon_pipe_message
{
    struct horizon_pipe_message *next;
    size_t size, offset;
    unsigned char data[];
};

struct horizon_byte_pipe_ring
{
    unsigned char *data;
    size_t size, head, used;
    struct horizon_pipe_message *first, *last;
    size_t charged;
    unsigned int messages;
};

struct horizon_byte_pipe
{
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    struct horizon_byte_pipe_ring incoming[2];
    unsigned int refs, open;
    int connected;
    int message_mode;
    unsigned int maxinstances, sharing;
    long long timeout;
    unsigned int read_mode[2];
};

static inline void horizon_pipe_clear_messages( struct horizon_byte_pipe_ring *ring )
{
    struct horizon_pipe_message *message;

    while ((message = ring->first))
    {
        ring->first = message->next;
        free( message );
    }
    ring->last = NULL;
    ring->used = ring->charged = ring->messages = 0;
}

struct horizon_byte_pipe_end
{
    struct horizon_byte_pipe *pipe;
    unsigned int side;
};

static inline size_t horizon_byte_pipe_quota( unsigned int size )
{
    return size < 4096 ? 4096 : size > 65536 ? 65536 : size;
}

static inline struct horizon_byte_pipe *horizon_byte_pipe_alloc( unsigned int in, unsigned int out )
{
    size_t in_size = horizon_byte_pipe_quota( in ), out_size = horizon_byte_pipe_quota( out );
    struct horizon_byte_pipe *pipe = calloc( 1, sizeof(*pipe) + in_size + out_size );

    if (!pipe) return NULL;
    pthread_mutex_init( &pipe->mutex, NULL );
    pthread_cond_init( &pipe->changed, NULL );
    pipe->incoming[0].data = (unsigned char *)(pipe + 1);
    pipe->incoming[0].size = in_size;
    pipe->incoming[1].data = pipe->incoming[0].data + in_size;
    pipe->incoming[1].size = out_size;
    pipe->refs = 2;
    pipe->open = 3;
    return pipe;
}

static inline void horizon_byte_pipe_close( struct horizon_byte_pipe_end *end )
{
    struct horizon_byte_pipe *pipe = end->pipe;
    int destroy;

    pthread_mutex_lock( &pipe->mutex );
    pipe->open &= ~(1u << end->side);
    destroy = !--pipe->refs;
    pthread_cond_broadcast( &pipe->changed );
    pthread_mutex_unlock( &pipe->mutex );
    if (!destroy) return;
    pthread_cond_destroy( &pipe->changed );
    pthread_mutex_destroy( &pipe->mutex );
    horizon_pipe_clear_messages( &pipe->incoming[0] );
    horizon_pipe_clear_messages( &pipe->incoming[1] );
    free( pipe );
}

static inline void horizon_byte_pipe_connect( struct horizon_byte_pipe *pipe )
{
    pthread_mutex_lock( &pipe->mutex );
    pipe->connected = 1;
    pthread_cond_broadcast( &pipe->changed );
    pthread_mutex_unlock( &pipe->mutex );
}

static inline int horizon_pipe_message_fits( const struct horizon_byte_pipe_ring *ring, size_t size )
{
    return !ring->first || (ring->charged < ring->size &&
        size <= ring->size - ring->charged && sizeof(struct horizon_pipe_message) <= ring->size - ring->charged - size);
}

static inline short horizon_byte_pipe_ready_locked( const struct horizon_byte_pipe_end *end, short events )
{
    const struct horizon_byte_pipe *pipe = end->pipe;
    const struct horizon_byte_pipe_ring *read = &pipe->incoming[end->side];
    const struct horizon_byte_pipe_ring *write = &pipe->incoming[1 - end->side];
    short result = 0;

    if (!(pipe->open & (1u << (1 - end->side)))) return (events & POLLIN) | POLLHUP | POLLERR;
    if (!pipe->connected) return 0;
    if ((events & POLLIN) && (read->used || read->messages)) result |= POLLIN;
    if ((events & POLLOUT) && (pipe->message_mode ? write->charged < write->size : write->used < write->size))
        result |= POLLOUT;
    return result;
}

/* Nonblocking descriptors let Wine queue overlapped operations in the server. */
static inline ssize_t horizon_byte_pipe_read_ex( struct horizon_byte_pipe_end *end, void *data,
                                                size_t size, int *more )
{
    struct horizon_byte_pipe *pipe = end->pipe;
    struct horizon_byte_pipe_ring *ring = &pipe->incoming[end->side];
    size_t count, first;

    if (more) *more = 0;
    if (!size) return 0;
    pthread_mutex_lock( &pipe->mutex );
    if (pipe->message_mode && pipe->connected && ring->first)
    {
        size_t total = 0;

        do
        {
            struct horizon_pipe_message *message = ring->first;
            count = message->size - message->offset;
            if (count > size - total) count = size - total;
            memcpy( (unsigned char *)data + total, message->data + message->offset, count );
            message->offset += count;
            ring->used -= count;
            total += count;
            if (message->offset == message->size)
            {
                ring->first = message->next;
                if (!ring->first) ring->last = NULL;
                ring->charged -= sizeof(*message) + message->size;
                --ring->messages;
                free( message );
            }
            else if (pipe->read_mode[end->side] && more) *more = 1;
            if (pipe->read_mode[end->side]) break;
        } while (ring->first && total < size);
        pthread_cond_broadcast( &pipe->changed );
        pthread_mutex_unlock( &pipe->mutex );
        return total;
    }
    count = pipe->connected ? ring->used : 0;
    if (!count)
    {
        int eof = !(pipe->open & (1u << (1 - end->side)));
        pthread_mutex_unlock( &pipe->mutex );
        errno = eof ? EPIPE : EAGAIN;
        return -1;
    }
    if (count > size) count = size;
    first = ring->size - ring->head;
    if (first > count) first = count;
    memcpy( data, ring->data + ring->head, first );
    memcpy( (unsigned char *)data + first, ring->data, count - first );
    ring->head = (ring->head + count) % ring->size;
    ring->used -= count;
    pthread_cond_broadcast( &pipe->changed );
    pthread_mutex_unlock( &pipe->mutex );
    return count;
}

static inline ssize_t horizon_byte_pipe_read( struct horizon_byte_pipe_end *end, void *data, size_t size )
{
    return horizon_byte_pipe_read_ex( end, data, size, NULL );
}

static inline ssize_t horizon_byte_pipe_write( struct horizon_byte_pipe_end *end, const void *data, size_t size )
{
    struct horizon_byte_pipe *pipe = end->pipe;
    struct horizon_byte_pipe_ring *ring = &pipe->incoming[1 - end->side];
    size_t count, tail, first;

    if (!size && !pipe->message_mode) return 0;
    pthread_mutex_lock( &pipe->mutex );
    if (!(pipe->open & (1u << (1 - end->side))))
    {
        pthread_mutex_unlock( &pipe->mutex );
        errno = EPIPE;
        return -1;
    }
    if (pipe->message_mode)
    {
        struct horizon_pipe_message *message;
        size_t charge;

        if (size > (size_t)-1 - sizeof(*message))
        {
            pthread_mutex_unlock( &pipe->mutex );
            errno = ENOMEM;
            return -1;
        }
        charge = sizeof(*message) + size;
        if (!pipe->connected || !horizon_pipe_message_fits( ring, size ))
        {
            pthread_mutex_unlock( &pipe->mutex );
            errno = EAGAIN;
            return -1;
        }
        if (!(message = malloc( charge )))
        {
            pthread_mutex_unlock( &pipe->mutex );
            errno = ENOMEM;
            return -1;
        }
        message->next = NULL;
        message->size = size;
        message->offset = 0;
        memcpy( message->data, data, size );
        if (ring->last) ring->last->next = message;
        else ring->first = message;
        ring->last = message;
        ring->used += size;
        ring->charged += charge;
        ++ring->messages;
        pthread_cond_broadcast( &pipe->changed );
        pthread_mutex_unlock( &pipe->mutex );
        return size;
    }
    count = pipe->connected ? ring->size - ring->used : 0;
    if (!count)
    {
        pthread_mutex_unlock( &pipe->mutex );
        errno = EAGAIN;
        return -1;
    }
    if (count > size) count = size;
    tail = (ring->head + ring->used) % ring->size;
    first = ring->size - tail;
    if (first > count) first = count;
    memcpy( ring->data + tail, data, first );
    memcpy( ring->data, (const unsigned char *)data + first, count - first );
    ring->used += count;
    pthread_cond_broadcast( &pipe->changed );
    pthread_mutex_unlock( &pipe->mutex );
    return count;
}

#endif
