#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "zlib.h"

/* HTTP gzip decompress */
int httpgzdecompress(Byte *zdata, uLong nzdata, Byte *data, uLong *ndata) {
    int err = 0;
    z_stream d_stream = {0}; /* decompression stream */
    static char dummy_head[2] =
    {
        0x8 + 0x7 * 0x10,
        (((0x8 + 0x7 * 0x10) * 0x100 + 30) / 31 * 31) & 0xFF,
    };
    d_stream.zalloc = Z_NULL;
    d_stream.zfree = Z_NULL;
    d_stream.opaque = Z_NULL;
    d_stream.next_in  = zdata;
    d_stream.avail_in = 0;
    d_stream.next_out = data;
    if(inflateInit2(&d_stream, 47) != Z_OK) {
        return -1;
    }
    while (d_stream.total_out < *ndata && d_stream.total_in < nzdata) {
        d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
        if((err = inflate(&d_stream, Z_NO_FLUSH)) == Z_STREAM_END) {
            break;
        }
        if(err != Z_OK ) {
            if(err == Z_DATA_ERROR) {
                d_stream.next_in = (Bytef*) dummy_head;
                d_stream.avail_in = sizeof(dummy_head);
                if((err = inflate(&d_stream, Z_NO_FLUSH)) != Z_OK) {
                    return -1;
                }
            } else {
                return -1;
            }
        }
    }
    if(inflateEnd(&d_stream) != Z_OK)  {
        return -1;
    }
    *ndata = d_stream.total_out;
    return 0;
}

int httpgzinit(z_stream* d_stream) {
    d_stream->zalloc = Z_NULL;
    d_stream->zfree = Z_NULL;
    d_stream->opaque = Z_NULL;
    d_stream->next_in  = 0;
    d_stream->avail_in = 0;

    if(inflateInit2(d_stream, 47) != Z_OK) {
        return -1;
    }

    return 0;
}

//http://stackoverflow.com/questions/17872152/decompress-with-gz-functions-succeeded-but-failed-with-inflate-functions-using
int httpgzread(z_stream* d_stream, Byte *zdata, uLong nzdata, Byte *data, uLong ndata) {
    static char dummy_head[2] =
    {
        0x8 + 0x7 * 0x10,
        (((0x8 + 0x7 * 0x10) * 0x100 + 30) / 31 * 31) & 0xFF,
    };
    int err = 0;
    d_stream->next_in = zdata;
    d_stream->next_out = data;
    while(d_stream->total_out < ndata && d_stream->total_in < nzdata) {
        d_stream->avail_in = d_stream->avail_out = 1;
        if((err = inflate(d_stream, Z_NO_FLUSH)) == Z_STREAM_END) {
            break;
        }
        if(err != Z_OK ) {
            printf("not ok\n");
            if(err == Z_DATA_ERROR) {
                d_stream->next_in = (Bytef*) dummy_head;
                d_stream->avail_in = sizeof(dummy_head);
                if((err = inflate(d_stream, Z_NO_FLUSH)) != Z_OK) {
                    printf("dummy\n");
                    return -1;
                }
            } else {
                return -1;
            }
        }
    }

    //assert(d_stream->total_in == nzdata);
    //*odata = d_stream->total_out;
    return 0;
}

int httpgzend(z_stream* d_stream) {
    if(inflateEnd(d_stream) != Z_OK) {
        return -1;
    }

    return 0;
}


