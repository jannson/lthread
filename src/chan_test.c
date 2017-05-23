#ifdef CHAN_TEST
#undef __STRICT_ANSI__

//copy from lthread
#define DEFINE_LTHREAD (lthread_set_funcname(__func__))
typedef struct lthread lthread_t;
typedef struct lthread_cond lthread_cond_t;
typedef struct lthread_mutex lthread_mutex_t;
void    lthread_run(void);
void lthread_set_sched_cpu(size_t cpu);

int passed = 0;

void assert_true_line(int expression, lthread_chan_t* chan, char* msg, int line)
{
    if (!expression)
    {
        chan_dispose(chan);
        if(0 == line) {
            fprintf(stderr, "Assertion failed: %s\n", msg);
        } else {
            fprintf(stderr, "Assertion failed: %s line=%d\n", msg, line);
        }
        exit(1);
    }
}

void assert_true(int expression, lthread_chan_t* chan, char* msg)
{
    assert_true_line(expression, chan, msg, 0);
}

void pass(void)
{
    printf(".");
    fflush(stdout);
    passed++;
}

void wait_for_reader(lthread_chan_t* chan)
{
    for (;;)
    {
        int send = chan->recv_count > 0;
        if (send) break;
        lthread_sleep(0);
    }
}

void wait_for_writer(lthread_chan_t* chan)
{
    for (;;)
    {
        int recv = chan->send_count > 0;
        if (recv) break;
        lthread_sleep(0);
    }
}

void test_chan_init_buffered(void)
{
    size_t size = 5;
    lthread_chan_t* chan = chan_init(size);

    assert_true(chan->queue != NULL, chan, "Queue is NULL");
    assert_true((size_t) chan->queue->capacity == size, chan, "Size is not 5");
    assert_true(!chan->closed, chan, "Chan is closed");

    chan_dispose(chan);
    pass();
}

void test_chan_init_unbuffered(void)
{
    lthread_chan_t* chan = chan_init(0);

    assert_true(chan->queue == NULL, chan, "Queue is not NULL");
    assert_true(!chan->closed, chan, "Chan is closed");

    chan_dispose(chan);
    pass();
}

void test_chan_init(void)
{
    test_chan_init_buffered();
    test_chan_init_unbuffered();
}

void test_chan_close(void)
{
    lthread_chan_t* chan = chan_init(0);

    assert_true(!chan->closed, chan, "Chan is closed");
    assert_true(!chan_is_closed(chan), chan, "Chan is closed");
    assert_true(chan_close(chan) == 0, chan, "Close failed");
    assert_true(chan->closed, chan, "Chan is not closed");
    assert_true(chan_close(chan) == -1, chan, "Close succeeded");
    assert_true(chan_is_closed(chan), chan, "Chan is not closed");

    chan_dispose(chan);
    pass();
}

void test_chan_send_buffered(void)
{
    lthread_chan_t* chan = chan_init(1);
    void* msg = "foo";

    assert_true(chan_size(chan) == 0, chan, "Queue is not empty");
    assert_true(chan_send(chan, msg) == 0, chan, "Send failed");
    assert_true(chan_size(chan) == 1, chan, "Queue is empty");

    chan_dispose(chan);
    pass();
}

void receiver(void* chan)
{
    void* msg;
    chan_recv(chan, &msg);
}

void test_chan_send_unbuffered(void)
{
    lthread_chan_t* chan = chan_init(0);
    void* msg = "foo";
    lthread_t* th;
    lthread_create(&th, receiver, chan);

    wait_for_reader(chan);

    assert_true(chan_size(chan) == 0, chan, "Chan size is not 0");
    assert_true(!chan->send_count, chan, "Chan has sender");
    assert_true(chan_send(chan, msg) == 0, chan, "Send failed");
    assert_true(!chan->send_count, chan, "Chan has sender");
    assert_true(chan_size(chan) == 0, chan, "Chan size is not 0");

    lthread_join(th, NULL, LTHREAD_FOREVER);
    chan_dispose(chan);
    pass();
}

void test_chan_send(void)
{
    test_chan_send_buffered();
    test_chan_send_unbuffered();
}

void test_chan_recv_buffered(void)
{
    lthread_chan_t* chan = chan_init(1);
    void* msg = "foo";

    assert_true(chan_size(chan) == 0, chan, "Queue is not empty");
    chan_send(chan, msg);
    assert_true(chan_size(chan) == 1, chan, "Queue is empty");
    void* received;
    assert_true(chan_recv(chan, &received) == 0, chan, "Recv failed");
    assert_true_line(msg == received, chan, "Messages are not equal", __LINE__);
    assert_true(chan_size(chan) == 0, chan, "Queue is not empty");

    chan_dispose(chan);
    pass();
}

void sender(void* chan)
{
    chan_send(chan, "foo");
}

void sleep_sender(void* chan)
{
    lthread_sleep(2000);
    chan_send(chan, "foo");
}

void test_chan_recv_unbuffered(void)
{
    lthread_chan_t* chan = chan_init(0);
    lthread_t* th;
    lthread_create(&th, sender, chan);

    assert_true(chan_size(chan) == 0, chan, "Chan size is not 0");
    assert_true(!chan->recv_count, chan, "Chan has reader");
    void *msg;
    assert_true(chan_recv(chan, &msg) == 0, chan, "Recv failed");
    assert_true_line(strcmp(msg, "foo") == 0, chan, "Messages are not equal", __LINE__);
    assert_true(!chan->recv_count, chan, "Chan has reader");
    assert_true(chan_size(chan) == 0, chan, "Chan size is not 0");

    lthread_join(th, NULL, LTHREAD_FOREVER);
    chan_dispose(chan);
    pass();
}

void test_chan_recv(void)
{
    test_chan_recv_buffered();
    test_chan_recv_unbuffered();
}

void test_chan_select_recv(void)
{
    lthread_chan_t* chan1 = chan_init(0);
    lthread_chan_t* chan2 = chan_init(1);
    lthread_chan_t* chans[2] = {chan1, chan2};
    void* msgs[2];
    lthread_sel_t* sel = lthread_sel_create();
    void* msg = "foo";

    chan_send(chan2, msg);

    switch(chan_select(sel, chans, msgs, 0, 2, 0))
    {
        case 0:
            chan_dispose(chan1);
            chan_dispose(chan2);
            fprintf(stderr, "Received on wrong channel line:%d\n", __LINE__);
            exit(1);
        case 1:
            if (strcmp(msgs[1], msg) != 0)
            {
                chan_dispose(chan1);
                chan_dispose(chan2);
                fprintf(stderr, "Messages are not equal line=%d\n", __LINE__);
                exit(1);
            }
            break;
        default:
            chan_dispose(chan1);
            chan_dispose(chan2);
            fprintf(stderr, "Received on no channels line:%d\n", __LINE__);
            exit(1);
    }

    lthread_t* th;
    lthread_create(&th, sender, chan1);
    wait_for_writer(chan1);

    switch(chan_select(sel, chans, msgs, 0, 2, 0))
    {
        case 0:
            if (strcmp(msgs[0], "foo") != 0)
            {
                chan_dispose(chan1);
                chan_dispose(chan2);
                fprintf(stderr, "Messages are not equal line=%d\n", __LINE__);
                exit(1);
            }
            break;
        case 1:
            chan_dispose(chan1);
            chan_dispose(chan2);
            fprintf(stderr, "Received on wrong channel line=%d\n", __LINE__);
            exit(1);
        default:
            chan_dispose(chan1);
            chan_dispose(chan2);
            fprintf(stderr, "Received on no channels line=%d\n", __LINE__);
            exit(1);
    }

    switch(chan_select(sel, &chan2, msg, 0, 1, 0))
    {
        case 0:
            chan_dispose(chan1);
            chan_dispose(chan2);
            fprintf(stderr, "Received on channel\n");
            exit(1);
        default:
            break;
    }

    lthread_join(th, NULL, LTHREAD_FOREVER);
    chan_dispose(chan1);
    chan_dispose(chan2);
    lthread_sel_dispose(sel);
    pass();
}

void test_chan_select_send(void)
{
    lthread_chan_t* chan1 = chan_init(0);
    lthread_chan_t* chan2 = chan_init(1);
    lthread_chan_t* chans[2] = {chan1, chan2};
    lthread_sel_t* sel = lthread_sel_create();
    void* msg[] = {"foo", "bar"};

    switch(chan_select(sel, chans, msg, 2, 0, 0))
    {
        case 0:
            chan_dispose(chan1);
            chan_dispose(chan2);
            fprintf(stderr, "Sent on wrong channel");
            exit(1);
        case 1:
            break;
        default:
            chan_dispose(chan1);
            chan_dispose(chan2);
            fprintf(stderr, "Sent on no channels");
            exit(1);
    }

    void* recv;
    chan_recv(chan2, &recv);
    if (strcmp(recv, "bar") != 0)
    {
        chan_dispose(chan1);
        chan_dispose(chan2);
        fprintf(stderr, "Messages are not equal line=%d\n", __LINE__);
        exit(1);
    }

    chan_send(chan2, "foo");

    lthread_t* th;
    lthread_create(&th, receiver, chan1);
    wait_for_reader(chan1);

    switch(chan_select(sel, chans, msg, 2, 0, 0))
    {
        case 0:
            break;
        case 1:
            chan_dispose(chan1);
            chan_dispose(chan2);
            fprintf(stderr, "Sent on wrong channel");
            exit(1);
        default:
            chan_dispose(chan1);
            chan_dispose(chan2);
            fprintf(stderr, "Sent on no channels");
            exit(1);
    }

    switch(chan_select(sel, &chan1, msg, 1, 0, 0))
    {
        case 0:
            chan_dispose(chan1);
            chan_dispose(chan2);
            fprintf(stderr, "Sent on channel");
            exit(1);
        default:
            break;
    }

    lthread_join(th, NULL, LTHREAD_FOREVER);
    chan_dispose(chan1);
    chan_dispose(chan2);
    lthread_sel_dispose(sel);
    pass();
}

void test_chan_select(void)
{
    test_chan_select_recv();
    test_chan_select_send();
}

void test_chan_int(void)
{
    lthread_chan_t* chan = chan_init(1);
    int s = 12345, r = 0;
    chan_send_int(chan, s);
    chan_recv_int(chan, &r);
    assert_true(s == r, chan, "Wrong value of int(12345)");

    int32_t s32 = 12345, r32 = 0;
    chan_send_int32(chan, s32);
    chan_recv_int32(chan, &r32);
    assert_true(s32 == r32, chan, "Wrong value of int32(12345)");

    int64_t s64 = 12345, r64 = 0;
    chan_send_int64(chan, s64);
    chan_recv_int64(chan, &r64);
    assert_true(s64 == r64, chan, "Wrong value of int64(12345)");

    chan_dispose(chan);
    pass();
}

void test_chan_double(void)
{
    lthread_chan_t* chan = chan_init(1);
    double s = 123.45, r = 0;
    chan_send_double(chan, s);
    chan_recv_double(chan, &r);
    assert_true(s == r, chan, "Wrong value of double(123.45)");

    chan_dispose(chan);
    pass();
}

void test_chan_buf(void)
{
    lthread_chan_t* chan = chan_init(1);
    char s[256], r[256];
    strcpy(s, "hello world");
    chan_send_buf(chan, s, sizeof(s));
    strcpy(s, "Hello World");
    chan_recv_buf(chan, &r, sizeof(s));
    assert_true(memcmp(s, r, sizeof(s)), chan, "Wrong value of buf");

    chan_dispose(chan);
    pass();
}

void test_chan_multi(void)
{
    lthread_chan_t* chan = chan_init(5);
    lthread_t* th[100];
    for (int i = 0; i < 50; ++i)
    {
       lthread_create(&th[i], sender, chan);
    }

    for (;;)
    {
       int all_waiting = chan->send_count == 45;
       if (all_waiting) break;
       lthread_sleep(0);
    }

    for (int i = 50; i < 100; ++i)
    {
       lthread_create(&th[i], receiver, chan);
    }

    for (int i = 0; i < 100; ++i)
    {
       lthread_join(th[i], NULL, LTHREAD_FOREVER);
    }

    chan_dispose(chan);
    pass();
}

lthread_chan_t *test_multi_block_chans[3];
static void test_multi_block(void* arg)
{
    int i = (long)arg;
    lthread_chan_t **chans = test_multi_block_chans;
    lthread_sel_t *sel = lthread_sel_create();
    lthread_chan_t *sel_chans[3];
    void* msgs[3];
    int ret;

    if(0 == i) {
        //recv; a,b send: c
        sel_chans[0] = chans[2];
        sel_chans[1] = chans[0];
        sel_chans[2] = chans[1];
        msgs[0] = (void*)3;
        ret = chan_select(sel, sel_chans, msgs, 1, 2, LTHREAD_FOREVER);
        fprintf(stderr, "i=%d msg[%d]=%ld\n", i, ret, (long)msgs[ret]);
        assert(ret == 1 && msgs[1] == (void*)1);
        chan_close(sel_chans[0]);
    } else if(1 == i) {
        //recv; b send: a
        sel_chans[0] = chans[0];
        sel_chans[1] = chans[1];
        msgs[0] = (void*)1;
        ret = chan_select(sel, sel_chans, msgs, 1, 1, LTHREAD_FOREVER);
        fprintf(stderr, "i=%d msg[%d]=%ld\n", i, ret, (long)msgs[ret]);
        assert(ret == 0 && msgs[0] == (void*)1);
    } else if(2 == i) {
        //recv; c send: a,b
        sel_chans[0] = chans[0];
        sel_chans[1] = chans[1];
        sel_chans[2] = chans[2];
        msgs[0] = (void*)5;
        msgs[1] = (void*)6;
        ret = chan_select(sel, sel_chans, msgs, 2, 1, LTHREAD_FOREVER);
        fprintf(stderr, "i=%d msg[%d]=%ld\n", i, ret, (long)msgs[ret]);
    }

    lthread_sel_dispose(sel);
}

static void test_chan_multi_block(void) {
    lthread_chan_t **chans = test_multi_block_chans;
    lthread_t* th[3];
    int i;

    for(i = 0; i < 3; i++) {
        chans[i] = chan_init(0);
        lthread_create(&th[i], test_multi_block, (void*)(long)i);
    }

    for(i = 0; i < 3; i++) {
       lthread_join(th[i], NULL, LTHREAD_FOREVER);
    }

    for(i = 0; i < 3; i++) {
        chan_dispose(chans[i]);
    }

}

static void test_multi_recv(void* arg) {
    lthread_chan_t *chan = (lthread_chan_t*)arg;
    void* data = NULL;
    int ret = 0;

    do {
        ret = chan_recv(chan, &data);
        fprintf(stderr, "%s ret=%d data=%ld\n", __FUNCTION__, ret, (long)data);
    } while(0 == ret && data != NULL);
}

static void test_chan_multi_close(void) {
    lthread_chan_t *chan;
    lthread_t* th;
    int i = 0;

    chan = chan_init(10);
    lthread_create(&th, test_multi_recv, chan);
    for(i = 0; i < 30; i++) {
        chan_send(chan, (void*)(long)(i+1));
    }

    chan_close(chan);

    lthread_join(th, NULL, LTHREAD_FOREVER);
    chan_dispose(chan);
}

static void lt_main1(void *arg) {
    DEFINE_LTHREAD;
    lthread_detach();

    test_chan_init();
    test_chan_close();
    test_chan_send();
    test_chan_recv();
    test_chan_select();
    test_chan_int();
    test_chan_double();
    test_chan_buf();
    test_chan_multi();
    test_chan_multi_block();
    test_chan_multi_close();
    printf("\n%d passed\n", passed);
}

int main(void) {
    lthread_t *lt1 = NULL;
    lthread_create(&lt1, (void*)lt_main1, (void*)NULL);
    lthread_run();

    return 0;
}
#endif
