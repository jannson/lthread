#ifndef __LTHREAD_CHAN_H_
#define __LTHREAD_CHAN_H_

#include <stdint.h>

typedef struct lthread_chan lthread_chan_t;
typedef struct lthread_sel lthread_sel_t;

// create a blocked selector for chans
lthread_sel_t *lthread_sel_create(void);

// release the block selector
void lthread_sel_dispose(lthread_sel_t* sel);

// Allocates and returns a new channel. The capacity specifies whether the
// channel should be buffered or not. A capacity of 0 will create an unbuffered
// channel. Sets errno and returns NULL if initialization failed.
lthread_chan_t* chan_init(size_t capacity);

// Releases the channel resources.
void chan_dispose(lthread_chan_t* chan);

// Once a channel is closed, data cannot be sent into it. If the channel is
// buffered, data can be read from it until it is empty, after which reads will
// return an error code. Reading from a closed channel that is unbuffered will
// return an error code. Closing a channel does not release its resources. This
// must be done with a call to chan_dispose. Returns 0 if the channel was
// successfully closed, -1 otherwise.
int chan_close(lthread_chan_t* chan);

// Returns 0 if the channel is open and 1 if it is closed.
int chan_is_closed(lthread_chan_t* chan);

// Sends a value into the channel. If the channel is unbuffered, this will
// block until a receiver receives the value. If the channel is buffered and at
// capacity, this will block until a receiver receives a value. Returns 0 if
// the send succeeded or -1 if it failed.
int chan_send(lthread_chan_t* chan, void* data);

// Receives a value from the channel. This will block until there is data to
// receive. Returns 0 if the receive succeeded or -1 if it failed.
int chan_recv(lthread_chan_t* chan, void** data);

// Returns the number of items in the channel buffer. If the channel is
// unbuffered, this will return 0.
int chan_size(lthread_chan_t* chan);

// send without block
int chan_send_noblock(lthread_chan_t* chan, void* data);

//timeout 0: noblock
//send first, recv second
int chan_select(struct lthread_sel *sel, struct lthread_chan* chans[], void **msgs, int wchan_count, int rchan_count, uint64_t timeout);

// Typed interface to send/recv chan.
int chan_send_int32(lthread_chan_t*, int32_t);
int chan_send_int64(lthread_chan_t*, int64_t);
#if ULONG_MAX == 4294967295UL
# define chan_send_int(c, d) chan_send_int64(c, d)
#else
# define chan_send_int(c, d) chan_send_int32(c, d)
#endif
int chan_send_double(lthread_chan_t*, double);
int chan_send_buf(lthread_chan_t*, void*, size_t);
int chan_recv_int32(lthread_chan_t*, int32_t*);
int chan_recv_int64(lthread_chan_t*, int64_t*);
#if ULONG_MAX == 4294967295UL
# define chan_recv_int(c, d) chan_recv_int64(c, d)
#else
# define chan_recv_int(c, d) chan_recv_int32(c, d)
#endif
int chan_recv_double(lthread_chan_t*, double*);
int chan_recv_buf(lthread_chan_t*, void*, size_t);

#endif

