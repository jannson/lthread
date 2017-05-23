#ifdef LTHREAD_CHAN_INT

#ifndef LTHREAD_FOREVER
#define LTHREAD_FOREVER ((uint64_t)(0x01FFFFFF))
#endif

static int chan_try_send(struct lthread_sel *sel, struct lthread_chan* chan, int idx, struct chan_wait** pwait);
static int chan_try_recv(struct lthread_sel *sel, struct lthread_chan* chan, int idx, struct chan_wait** pwait);
static void lthread_sel_init(struct lthread_sel* sel, int chans);
static int chan_block_select_(struct lthread_sel *sel, uint64_t timeout);
static void sel_remove_all_chans(struct lthread_sel * other_sel);

lthread_chan_t* chan_init(size_t capacity)
{
    lthread_chan_t* chan = (lthread_chan_t*) lthr_calloc(1, sizeof(lthread_chan_t));
    if (!chan)
    {
        errno = ENOMEM;
        return NULL;
    }

    TAILQ_INIT(&chan->sendq);
    TAILQ_INIT(&chan->recvq);

    if (capacity > 0)
    {
        chan->queue = queue_init(capacity);
    }

    return chan;
}

void chan_dispose(lthread_chan_t* chan)
{
    if (chan->queue != NULL)
    {
        queue_dispose(chan->queue);
    }

    lthr_free(chan);
}

int chan_close(lthread_chan_t* chan)
{
    struct lthread_sel *other_sel = NULL;
    struct chan_wait *wait, *tmp_wait;

    if (chan->closed)
    {
        // Channel already closed.
        errno = EPIPE;
        return -1;
    }
    else
    {
        // Otherwise close it.
        chan->closed = 1;
        TAILQ_FOREACH_SAFE(wait, &chan->recvq, entry, tmp_wait) {
            other_sel = wait->sel;
            if(-1 == other_sel->target) {
                //found the other selector
                wait->sel->msgs[wait->idx] = NULL;
                other_sel->target = wait->idx;
                sel_remove_all_chans(other_sel);
                lthread_cond_signal(&other_sel->cond);
            }
        }
    }
    return 0;
}

// Returns 0 if the channel is open and 1 if it is closed.
int chan_is_closed(lthread_chan_t* chan)
{
    return chan->closed;
}

int chan_send(lthread_chan_t* chan, void* data)
{
    struct lthread_sel s, *sel;
    sel = &s;

    if (chan_is_closed(chan))
    {
        // Cannot send on closed channel.
        errno = EPIPE;
        return -1;
    }

    lthread_sel_init(sel, 1);
    sel->msgs = &data;
    sel->chans = &chan;
    sel->wchan_count = 1;
    sel->rchan_count = 0;
    return chan_block_select_(sel, LTHREAD_FOREVER);
}

int chan_send_noblock(lthread_chan_t* chan, void* data)
{
    struct lthread_sel s, *sel;
    sel = &s;

    if (chan_is_closed(chan))
    {
        // Cannot send on closed channel.
        errno = EPIPE;
        return -1;
    }

    lthread_sel_init(sel, 1);
    sel->msgs = &data;
    sel->chans = &chan;
    sel->wchan_count = 1;
    sel->rchan_count = 0;
    return chan_block_select_(sel, 0);
}

int chan_recv(lthread_chan_t* chan, void** data)
{
    struct lthread_sel s, *sel;
    sel = &s;

    lthread_sel_init(sel, 1);
    sel->msgs = data;
    sel->chans = &chan;
    sel->wchan_count = 0;
    sel->rchan_count = 1;
    return chan_block_select_(sel, LTHREAD_FOREVER);
}

int chan_size(lthread_chan_t* chan)
{
    int size = 0;
    if (chan->queue != NULL)
    {
        size = chan->queue->size;
    }
    return size;
}

//timeout : 0 means noblock select
int chan_select(struct lthread_sel *sel, struct lthread_chan* chans[], void **msgs, int wchan_count, int rchan_count, uint64_t timeout)
{
    sel->chans = chans;
    sel->msgs = msgs;
    sel->wchan_count = wchan_count;
    sel->rchan_count = rchan_count;
    return chan_block_select_(sel, timeout);
}

int chan_send_int32(lthread_chan_t* chan, int32_t data)
{
    int32_t* wrapped = lthr_malloc(sizeof(int32_t));
    if (!wrapped)
    {
        return -1;
    }

    *wrapped = data;

    int success = chan_send(chan, wrapped);
    if (success != 0)
    {
        lthr_free(wrapped);
    }

    return success;
}

int chan_recv_int32(lthread_chan_t* chan, int32_t* data)
{
    int32_t* wrapped = NULL;
    int success = chan_recv(chan, (void*) &wrapped);
    if (wrapped != NULL)
    {
        *data = *wrapped;
        lthr_free(wrapped);
    }

    return success;
}

int chan_send_int64(lthread_chan_t* chan, int64_t data)
{
    int64_t* wrapped = lthr_malloc(sizeof(int64_t));
    if (!wrapped)
    {
        return -1;
    }

    *wrapped = data;

    int success = chan_send(chan, wrapped);
    if (success != 0)
    {
        lthr_free(wrapped);
    }

    return success;
}

int chan_recv_int64(lthread_chan_t* chan, int64_t* data)
{
    int64_t* wrapped = NULL;
    int success = chan_recv(chan, (void*) &wrapped);
    if (wrapped != NULL)
    {
        *data = *wrapped;
        lthr_free(wrapped);
    }

    return success;
}

int chan_send_double(lthread_chan_t* chan, double data)
{
    double* wrapped = lthr_malloc(sizeof(double));
    if (!wrapped)
    {
        return -1;
    }

    *wrapped = data;

    int success = chan_send(chan, wrapped);
    if (success != 0)
    {
        lthr_free(wrapped);
    }

    return success;
}

int chan_recv_double(lthread_chan_t* chan, double* data)
{
    double* wrapped = NULL;
    int success = chan_recv(chan, (void*) &wrapped);
    if (wrapped != NULL)
    {
        *data = *wrapped;
        lthr_free(wrapped);
    }

    return success;
}

int chan_send_buf(lthread_chan_t* chan, void* data, size_t size)
{
    void* wrapped = lthr_malloc(size);
    if (!wrapped)
    {
        return -1;
    }

    memcpy(wrapped, data, size);

    int success = chan_send(chan, wrapped);
    if (success != 0)
    {
        lthr_free(wrapped);
    }

    return success;
}

int chan_recv_buf(lthread_chan_t* chan, void* data, size_t size)
{
    void* wrapped = NULL;
    int success = chan_recv(chan, (void*) &wrapped);
    if (wrapped != NULL)
    {
        memcpy(data, wrapped, size);
        lthr_free(wrapped);
    }

    return success;
}

struct lthread_sel* lthread_sel_create(void)
{
    struct lthread_sel *sel = NULL;
    if((sel = lthr_calloc(1, sizeof(struct lthread_sel))) == NULL) {
        return NULL;
    }
    lthread_sel_init(sel, LTHR_SEL_CHANS);
    return sel;
}

static void lthread_sel_init(struct lthread_sel* sel, int chans)
{
    int i;
    lthread_cond_init(&sel->cond);
    for(i = 0; i < chans; i++) {
        sel->waits[i].idx = i;
        sel->waits[i].sel = sel;
    }
    sel->target = -1;
}

void lthread_sel_dispose(lthread_sel_t *sel)
{
    lthr_free(sel);
}

static int chan_block_select_(struct lthread_sel *sel, uint64_t timeout)
{
    struct lthread_chan *chan;
    struct chan_wait *wait = NULL;
    int i, n;
    for(i = 0; i < sel->wchan_count; i++) {
        chan = sel->chans[i];
        if(chan_try_send(sel, chan, i, &wait)) {
            //found waiting reader
            break;
        }
    }

    if(-1 == sel->target) {
        n = sel->rchan_count + sel->wchan_count;
        for(i = sel->wchan_count; i < n; i++) {
            chan = sel->chans[i];
            if(chan_try_recv(sel, chan, i, &wait)) {
                break;
            }
        }
    }

    if(-1 != sel->target) {
        if(wait != NULL) {
            lthread_cond_signal(&wait->sel->cond);
        }
        n = sel->target;
        sel->target = -1;
        return n;
    }

    if(0 == timeout) {
        //not timeout operation
        return -1;
    }

    //not ready yet, waiting
    for(i = 0; i < sel->wchan_count; i++) {
        chan = sel->chans[i];
        ++chan->send_count;
        TAILQ_INSERT_TAIL(&chan->sendq, &sel->waits[i], entry);
    }

    n = sel->rchan_count + sel->wchan_count;
    for(i = sel->wchan_count; i < n; i++) {
        chan = sel->chans[i];
        ++chan->recv_count;
        TAILQ_INSERT_TAIL(&chan->recvq, &sel->waits[i], entry);
    }

    if(0 != lthread_cond_wait(&sel->cond, timeout)) {
        if(sel->target < 0) {
            sel_remove_all_chans(sel);
            return -2;
        }
    }

    //when wakeup, sel already got it's message
    n = sel->target;
    sel->target = -1;//reset target
    return n;
}

static int chan_try_send(struct lthread_sel *sel, struct lthread_chan* chan, int idx, struct chan_wait** pwait)
{
    struct lthread_sel *other_sel = NULL;
    struct chan_wait *wait;
    int buffered = 0;

    assert(!chan->closed);
    if(NULL != chan->queue && chan->queue->size < chan->queue->capacity) {
        queue_add(chan->queue, sel->msgs[idx]);
        buffered = 1;
        sel->target = idx;
    }

    TAILQ_FOREACH(wait, &chan->recvq, entry) {
        other_sel = wait->sel;
        if(-1 == other_sel->target) {
            //found the other selector
            if(buffered) {
                wait->sel->msgs[wait->idx] = queue_remove(chan->queue);
            } else {
                wait->sel->msgs[wait->idx] = sel->msgs[idx];
                sel->target = idx;
            }
            break;
        }
    }

    if(NULL != other_sel && -1 == other_sel->target) {
        //remove all waiting of this selectors
        sel_remove_all_chans(other_sel);
        other_sel->target = wait->idx;
        *pwait = wait;
    }

    if(-1 != sel->target) {
        return 1;
    }

    return 0;
}

static int chan_try_recv(struct lthread_sel *sel, struct lthread_chan* chan, int idx, struct chan_wait** pwait)
{
    struct lthread_sel *other_sel = NULL;
    struct chan_wait *wait;
    int buffered = 0;

    if(chan->queue != NULL && chan->queue->size > 0) {
        sel->msgs[idx] = queue_remove(chan->queue);
        buffered = 1;
        sel->target = idx;
    }

    TAILQ_FOREACH(wait, &chan->sendq, entry) {
        other_sel = wait->sel;
        if(-1 == other_sel->target) {
            if(buffered) {
                queue_add(chan->queue, wait->sel->msgs[wait->idx]);
            } else {
                sel->msgs[idx] = wait->sel->msgs[wait->idx];
                sel->target = idx;
            }
        }

        break;
    }

    if(NULL != other_sel && -1 == other_sel->target) {
        //remove all waiting of this selectors
        sel_remove_all_chans(other_sel);
        other_sel->target = wait->idx;
        *pwait = wait;
    }

    if(-1 != sel->target) {
        return 1;
    }

    if(chan->closed) {
        sel->msgs[idx] = NULL;
        sel->target = idx;
        return 1;
    }

    return 0;
}

static void sel_remove_all_chans(struct lthread_sel * other_sel)
{
    int i, n;
    struct chan_wait *other_wait;
    struct lthread_chan *other_chan = NULL;
    for(i = 0; i < other_sel->wchan_count; i++) {
        other_wait = &other_sel->waits[i];
        other_chan = other_sel->chans[i];
        TAILQ_REMOVE(&other_chan->sendq, other_wait, entry);
        --other_chan->send_count;
    }
    n = other_sel->rchan_count + other_sel->wchan_count;
    for(i = other_sel->wchan_count; i < n; i++) {
        other_wait = &other_sel->waits[i];
        other_chan = other_sel->chans[i];
        TAILQ_REMOVE(&other_chan->recvq, other_wait, entry);
        --other_chan->recv_count;
    }
}

#ifdef CHAN_TEST
#include "chan_test.c"
#endif
#endif

